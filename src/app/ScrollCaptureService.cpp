#include "app/ScrollCaptureService.h"

#include "app/HistoryService.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/ScreenCapturer.h"
#include "ui/scroll/RegionIndicator.h"
#include "ui/scroll/ScrollCaptureBar.h"
#include "ui/scroll/ScrollPreview.h"

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace pixora {

namespace {
// ~30fps:帧间位移小 → 手动快速滚动也有充足重叠。Windows 走 DXGI 时
// 空转 tick 只是一次立即超时的查询;GDI 兜底时长时间无变化降到 15fps
constexpr int kFrameIntervalMs = 33;
constexpr int kIdleIntervalMs = 66;
constexpr int kIdleTicksBeforeSlow = 30; // 约 1 秒无变化才降帧

// 自动模式的"步":注入 → 拼接滚动中的每个变化帧 → 安静下来即结束
constexpr int kSettleTicks = 3;        // 连续无变化/无增长 tick 数 → 本步结束
constexpr int kStepNoMotionMs = 500;   // 注入后这么久仍无位移 → 本步无新内容
constexpr int kStepTimeoutMs = 1500;   // 单步上限(动画持续变化时兜底)
constexpr int kAutoFinishStreak = 3;   // 连续无新内容的步 → 判定到底
constexpr int kAutoFailStreak = 3;     // 连续失配的步 → 退回手动
constexpr double kAutoStepRatio = 0.6; // 每步目标位移 / 内容区高度(留 40% 重叠)
constexpr int kMaxNotches = 15;
constexpr int kMaxKeyPresses = 20;

// 未校准前的初始格数:小区域 1 格防止一步跳过重叠带,大区域最多 3 格
int initialNotches(int regionLogicalHeight) {
    return std::clamp(regionLogicalHeight / 250, 1, 3);
}

// 16 行等距采样的"近似相等":过滤桌面别处的更新(时钟、其他窗口)
// 与无关紧要的小变化。滚动会改动整个区域,拼接正确性不受影响。
bool sampledFrameEqual(const QImage& a, const QImage& b) {
    if (a.size() != b.size() || a.format() != b.format()) {
        return false;
    }
    constexpr int kRows = 16;
    const int last = a.height() - 1;
    for (int i = 0; i < kRows; ++i) {
        const int y = last * i / (kRows - 1);
        if (std::memcmp(a.constScanLine(y), b.constScanLine(y),
                        static_cast<size_t>(a.bytesPerLine())) != 0) {
            return false;
        }
    }
    return true;
}
} // namespace

ScrollCaptureService::ScrollCaptureService(IScreenCapturer& capturer,
                                           IInputInjector* injector,
                                           const SettingsService* settings,
                                           HistoryService* history, QObject* parent)
    : QObject(parent), capturer_(capturer), injector_(injector), output_(settings),
      history_(history) {
    timer_.setInterval(kFrameIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &ScrollCaptureService::tick);
}

ScrollCaptureService::~ScrollCaptureService() {
    teardownScroll();
}

void ScrollCaptureService::finish() {
    if (isActive()) {
        finishCapture(Outlet::Copy);
    }
}

void ScrollCaptureService::start(const QRect& regionGlobal) {
    if (isActive()) {
        return;
    }
    screen_ = QGuiApplication::screenAt(regionGlobal.center());
    if (!screen_) {
        screen_ = QGuiApplication::primaryScreen();
    }
    regionGlobal_ = regionGlobal & screen_->geometry(); // M3: 区域限单屏
    if (regionGlobal_.height() < 60 || regionGlobal_.width() < 60) {
        spdlog::warn("scroll capture: region too small, aborted");
        return;
    }

    // 保护带按 DPR 折算:滚动条约 17 逻辑 px,150%/200% 下物理宽度随之变大
    const qreal dpr = screen_->devicePixelRatio();
    Stitcher::Config config;
    config.rightGuard = std::max(24, qRound(20 * dpr));
    config.bottomGuard = std::max(12, qRound(10 * dpr));
    stitcher_ = Stitcher(config);
    stream_ = capturer_.openFrameStream(
        screen_, regionGlobal_.translated(-screen_->geometry().topLeft()));
    lastGrab_ = {};
    frames_ = 0;
    idleTicks_ = 0;
    limitReached_ = false;
    timer_.setInterval(kFrameIntervalMs);

    autoMode_ = false;
    stepActive_ = false;
    driver_ = Driver::Wheel;
    pxPerUnit_ = 0.0;
    stepScale_ = 1.0;
    movedSteps_ = 0;
    noNewStreak_ = 0;
    failStreak_ = 0;

    recordDir_.clear();
    recordIndex_ = 0;
    const QString recordRoot = qEnvironmentVariable("PIXORA_RECORD_FRAMES");
    if (!recordRoot.isEmpty()) {
        recordDir_ = QDir(recordRoot).filePath(
            QStringLiteral("case_%1").arg(QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd_HHmmss"))));
        QDir().mkpath(recordDir_);
        spdlog::info("recording scroll frames to {}", recordDir_.toStdString());
    }

    indicator_ = new RegionIndicator(regionGlobal_);
    indicator_->show();
    // 实时预览条:区域两侧都放不下(如全宽窗口)则不显示
    if (ScrollPreview::fitsBeside(regionGlobal_, screen_->virtualGeometry())) {
        preview_ = new ScrollPreview(regionGlobal_, screen_->virtualGeometry());
        preview_->show();
    } else {
        spdlog::info("scroll preview skipped: no room beside region");
    }
    lastPreviewMs_ = 0;
    bar_ = new ScrollCaptureBar(regionGlobal_, screen_->virtualGeometry(),
                                injector_ != nullptr);
    connect(bar_, &ScrollCaptureBar::finishRequested, this,
            [this] { finishCapture(Outlet::Copy); });
    connect(bar_, &ScrollCaptureBar::finishPinRequested, this,
            [this] { finishCapture(Outlet::Pin); });
    connect(bar_, &ScrollCaptureBar::finishSaveRequested, this,
            [this] { finishCapture(Outlet::Save); });
    connect(bar_, &ScrollCaptureBar::cancelRequested, this, [this] {
        spdlog::info("scroll capture cancelled");
        teardownScroll();
    });
    connect(bar_, &ScrollCaptureBar::undoRequested, this,
            &ScrollCaptureService::undoLastSegment);
    connect(bar_, &ScrollCaptureBar::autoToggled, this, &ScrollCaptureService::setAutoMode);
    bar_->show();

    timer_.start();
    spdlog::info("scroll capture started: region {}x{} on screen dpr {}",
                 regionGlobal_.width(), regionGlobal_.height(), dpr);
}

int ScrollCaptureService::logicalHeight() const {
    return qRound(stitcher_.resultHeight() / screen_->devicePixelRatio());
}

void ScrollCaptureService::tick() {
    bool changed = false;
    const QImage frame = stream_->grab(&changed);
    changed = changed && !frame.isNull();

    if (!stitcher_.active()) {
        if (changed) {
            beginStitch(frame);
        }
        return;
    }

    // 桌面别处的更新也会报告变化,再看一眼本区域
    if (changed && sampledFrameEqual(frame, lastGrab_)) {
        changed = false;
    }
    if (changed) {
        lastGrab_ = frame;
        idleTicks_ = 0;
    } else {
        ++idleTicks_;
    }
    adaptFrameRate();

    if (autoMode_ && injector_) {
        tickAuto(changed);
    } else if (changed && !limitReached_) {
        appendManual(frame);
    }
}

void ScrollCaptureService::beginStitch(const QImage& frame) {
    stitcher_.begin(frame);
    frames_ = 1;
    lastGrab_ = frame;
    recordFrame(frame);
    updatePreview(true);
    bar_->setProgress(logicalHeight());
    bar_->setStatus(tr("First frame captured, scroll the target window..."),
                    ScrollCaptureBar::Tone::Good);
}

// 低成本通道(DXGI)空转近乎免费,始终满帧率;GDI 兜底时
// 长时间无变化降帧省电,一有变化立即恢复
void ScrollCaptureService::adaptFrameRate() {
    if (stream_->accelerated()) {
        return;
    }
    const int wanted = (!autoMode_ && idleTicks_ >= kIdleTicksBeforeSlow) ? kIdleIntervalMs
                                                                         : kFrameIntervalMs;
    if (timer_.interval() != wanted) {
        timer_.setInterval(wanted);
    }
}

void ScrollCaptureService::appendManual(const QImage& frame) {
    recordFrame(frame);
    switch (stitcher_.append(frame)) {
    case Stitcher::AppendResult::Appended:
        onAppended();
        bar_->setStatus(tr("Keep scrolling; press F1 or Copy to finish"),
                        ScrollCaptureBar::Tone::Good);
        break;
    case Stitcher::AppendResult::NoNewContent:
        updatePreview(); // 原地刷新(懒加载完成)也可能改了尾部
        break;
    case Stitcher::AppendResult::MatchFailed:
        bar_->setStatus(tr("Could not align: scroll back a little and go slower"),
                        ScrollCaptureBar::Tone::Warning);
        break;
    case Stitcher::AppendResult::LimitReached:
        onLimitReached();
        break;
    }
}

void ScrollCaptureService::onAppended() {
    ++frames_;
    updatePreview();
    bar_->setProgress(logicalHeight());
    bar_->setUndoEnabled(stitcher_.canUndo());
}

void ScrollCaptureService::onLimitReached() {
    limitReached_ = true;
    if (autoMode_) {
        spdlog::info("scroll capture: max height reached, auto-finishing");
        finishCapture();
        return;
    }
    bar_->setStatus(tr("Maximum length reached; press Copy to finish"),
                    ScrollCaptureBar::Tone::Warning);
}

void ScrollCaptureService::undoLastSegment() {
    if (autoMode_) {
        bar_->setAutoChecked(false); // 经 autoToggled 关掉自动模式
    }
    if (!stitcher_.undo()) {
        return;
    }
    limitReached_ = false;
    --frames_;
    updatePreview(true);
    bar_->setProgress(logicalHeight());
    bar_->setUndoEnabled(stitcher_.canUndo());
    bar_->setStatus(tr("Last segment removed. Scroll back up a little to continue"),
                    ScrollCaptureBar::Tone::Neutral);
    spdlog::info("scroll capture: last segment undone");
}

void ScrollCaptureService::setAutoMode(bool enabled) {
    autoMode_ = enabled;
    stepActive_ = false; // 下个 tick 立即开始新的一步
    noNewStreak_ = 0;
    failStreak_ = 0;
    spdlog::info("scroll capture auto mode: {}", enabled ? "on" : "off");
    if (bar_) {
        bar_->setStatus(enabled ? tr("Auto-scrolling...")
                                : tr("Scroll the target window to keep stitching..."),
                        ScrollCaptureBar::Tone::Good);
    }
}

void ScrollCaptureService::updatePreview(bool force) {
    if (!preview_) {
        return;
    }
    // 节流:手动快速滚动时拼接可达每秒十几次,尾部拷贝+缩放别跟满
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && now - lastPreviewMs_ < 150) {
        return;
    }
    lastPreviewMs_ = now;
    const qreal dpr = screen_->devicePixelRatio();
    const int canvasWidthPhysical = qRound(regionGlobal_.width() * dpr);
    const QImage tail = stitcher_.tail(preview_->tailRowsFor(canvasWidthPhysical));
    preview_->updateContent(tail, logicalHeight());
}

void ScrollCaptureService::recordFrame(const QImage& frame) {
    if (recordDir_.isEmpty()) {
        return;
    }
    frame.save(QDir(recordDir_).filePath(
        QStringLiteral("frame_%1.png").arg(recordIndex_++, 4, 10, QLatin1Char('0'))));
}

// ---- 自动模式 ----

void ScrollCaptureService::tickAuto(bool changed) {
    if (!stepActive_) {
        startStep();
        return;
    }
    if (changed) {
        quietTicks_ = 0;
        recordFrame(lastGrab_);
        const int before = stitcher_.resultHeight();
        switch (stitcher_.append(lastGrab_)) {
        case Stitcher::AppendResult::Appended:
            stepGrowth_ += stitcher_.resultHeight() - before;
            stepFailed_ = false;
            stillFrames_ = 0;
            onAppended();
            break;
        case Stitcher::AppendResult::NoNewContent:
            ++stillFrames_;
            break;
        case Stitcher::AppendResult::MatchFailed:
            stepFailed_ = true;
            ++stillFrames_;
            break;
        case Stitcher::AppendResult::LimitReached:
            onLimitReached();
            return;
        }
    } else {
        ++quietTicks_;
    }

    const qint64 elapsed = stepClock_.elapsed();
    const bool settled = quietTicks_ >= kSettleTicks || stillFrames_ >= kSettleTicks;
    const bool done = stepGrowth_ > 0
                          ? settled || elapsed >= kStepTimeoutMs
                          : (settled && elapsed >= kStepNoMotionMs) || elapsed >= kStepTimeoutMs;
    if (done) {
        finishStep();
    }
}

int ScrollCaptureService::stepUnits() const {
    if (driver_ == Driver::PageDown) {
        return 1;
    }
    const int maxUnits = driver_ == Driver::Wheel ? kMaxNotches : kMaxKeyPresses;
    if (pxPerUnit_ <= 0.0) {
        return driver_ == Driver::Wheel ? initialNotches(regionGlobal_.height()) : 3;
    }
    const double target = kAutoStepRatio * stepScale_ * stitcher_.contentHeight();
    return std::clamp(static_cast<int>(std::lround(target / pxPerUnit_)), 1, maxUnits);
}

void ScrollCaptureService::injectUnits(int units, bool down) {
    const QPoint at = regionGlobal_.center();
    for (int i = 0; i < units; ++i) {
        switch (driver_) {
        case Driver::Wheel:
            // 逐格发送:有的应用无视 delta 大小,一条消息只滚一格
            injector_->sendScroll(at, down ? -120 : 120);
            break;
        case Driver::PageDown:
            injector_->sendKey(at, down ? Qt::Key_PageDown : Qt::Key_PageUp);
            break;
        case Driver::ArrowKeys:
            injector_->sendKey(at, down ? Qt::Key_Down : Qt::Key_Up);
            break;
        }
    }
}

void ScrollCaptureService::startStep() {
    stepUnits_ = stepUnits();
    injectUnits(stepUnits_, /*down=*/true);
    stepActive_ = true;
    stepClock_.start();
    stepGrowth_ = 0;
    stepFailed_ = false;
    quietTicks_ = 0;
    stillFrames_ = 0;
}

void ScrollCaptureService::switchDriver(Driver driver, const QString& status) {
    driver_ = driver;
    pxPerUnit_ = 0.0;
    stepScale_ = 1.0;
    movedSteps_ = 0;
    noNewStreak_ = 0;
    failStreak_ = 0;
    bar_->setStatus(status, ScrollCaptureBar::Tone::Good);
}

void ScrollCaptureService::finishStep() {
    stepActive_ = false;

    if (stepGrowth_ > 0) {
        noNewStreak_ = 0;
        failStreak_ = 0;
        ++movedSteps_;
        // 校准每格位移(滑动平均,抗平滑滚动的起止误差);到底前的
        // 最后一步位移偏小只会让下一步多滚几格,无害
        if (driver_ != Driver::PageDown && stepUnits_ > 0) {
            const double measured = static_cast<double>(stepGrowth_) / stepUnits_;
            pxPerUnit_ = pxPerUnit_ <= 0.0 ? measured : 0.5 * pxPerUnit_ + 0.5 * measured;
        }
        stepScale_ = std::min(1.0, stepScale_ * 1.25); // 失配后逐步恢复步幅
        bar_->setStatus(tr("Auto-scrolling..."), ScrollCaptureBar::Tone::Good);
        return;
    }

    if (stepFailed_) {
        // 一步滚过头(与上一帧无重叠):先退回这一步,再换小步
        if (driver_ == Driver::PageDown) {
            spdlog::info("scroll capture: PageDown overshoots region, switching to arrow keys");
            injectUnits(1, /*down=*/false);
            switchDriver(Driver::ArrowKeys,
                         tr("Page Down jumps past the region; driving with arrow keys..."));
            return;
        }
        if (++failStreak_ >= kAutoFailStreak) {
            bar_->setAutoChecked(false); // 经 autoToggled 关掉自动模式
            bar_->setStatus(tr("Auto-scroll lost alignment; switched back to manual, "
                               "please scroll by hand"),
                            ScrollCaptureBar::Tone::Warning);
            return;
        }
        injectUnits(stepUnits_, /*down=*/false);
        stepScale_ *= 0.5;
        bar_->setStatus(tr("Scrolled too far; stepping back and slowing down..."),
                        ScrollCaptureBar::Tone::Warning);
        return;
    }

    ++noNewStreak_;
    // 起步就毫无位移:该应用不吃这种输入,换下一种驱动
    if (movedSteps_ == 0 && noNewStreak_ >= 2 && driver_ != Driver::ArrowKeys) {
        if (driver_ == Driver::Wheel) {
            spdlog::info("scroll capture: wheel ineffective, switching to PageDown");
            switchDriver(Driver::PageDown, tr("Wheel events ignored, driving with PageDown..."));
        } else {
            spdlog::info("scroll capture: PageDown ineffective, switching to arrow keys");
            switchDriver(Driver::ArrowKeys,
                         tr("Page Down ignored; driving with arrow keys..."));
        }
        return;
    }
    if (noNewStreak_ >= kAutoFinishStreak) {
        spdlog::info("scroll capture: bottom reached, auto-finishing");
        finishCapture();
    }
}

// ---- 收尾 ----

void ScrollCaptureService::finishCapture(Outlet outlet) {
    timer_.stop();
    QImage result = stitcher_.result();
    const qreal dpr = screen_ ? screen_->devicePixelRatio() : 1.0;
    const QPoint regionTopLeft = regionGlobal_.topLeft();
    teardownScroll();
    if (result.isNull()) {
        spdlog::info("scroll capture finished with no content");
        return;
    }
    result.setDevicePixelRatio(dpr);
    spdlog::info("scroll capture finished: {}x{} px, {} frames", result.width(),
                 result.height(), frames_);
    if (!recordDir_.isEmpty()) {
        result.save(QDir(recordDir_).filePath(QStringLiteral("expected.png")));
    }
    if (history_) {
        history_->record(result);
    }

    switch (outlet) {
    case Outlet::Copy: {
        output_.copyToClipboard(result);
        emit copiedToClipboard(qRound(result.height() / dpr), result);
        const QString autoSaved = output_.autoSave(result);
        if (!autoSaved.isEmpty()) {
            emit savedToFile(autoSaved);
        }
        break;
    }
    case Outlet::Pin:
        emit pinCaptured(result, regionTopLeft); // 贴在原捕获区位置
        break;
    case Outlet::Save: {
        const QString path = output_.saveWithDialog(result);
        if (!path.isEmpty()) {
            emit savedToFile(path);
        }
        break;
    }
    }
}

void ScrollCaptureService::teardownScroll() {
    timer_.stop();
    if (indicator_) {
        indicator_->close();
        indicator_ = nullptr;
    }
    if (bar_) {
        bar_->close();
        bar_->deleteLater();
        bar_ = nullptr;
    }
    if (preview_) {
        preview_->close();
        preview_->deleteLater();
        preview_ = nullptr;
    }
    stream_.reset();
    lastGrab_ = {};
    stitcher_ = Stitcher(); // 释放画布与撤销历史
}

} // namespace pixora
