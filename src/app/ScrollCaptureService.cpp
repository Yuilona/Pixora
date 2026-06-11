#include "app/ScrollCaptureService.h"

#include "app/HistoryService.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/ScreenCapturer.h"
#include "ui/scroll/RegionIndicator.h"
#include "ui/scroll/ScrollCaptureBar.h"

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kFrameIntervalMs = 66;     // ~15fps
constexpr int kStableTimeoutMs = 800;    // 稳定等待超时(懒加载兜底)
constexpr int kAutoStepIntervalMs = 400; // 两次注入滚轮的最小间隔(节奏限制)
constexpr int kAutoFinishStreak = 3;     // 连续无新内容 → 判定到底
constexpr int kAutoFailStreak = 3;       // 连续匹配失败 → 退回手动

// 每步滚轮格数随区域高度自适应:小区域滚 1 格防止一步跳过重叠带,
// 大区域最多 3 格(浏览器一格约滚 100+ 逻辑像素)。
int autoScrollDelta(int regionLogicalHeight) {
    const int notches = std::clamp(regionLogicalHeight / 250, 1, 3);
    return -120 * notches;
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

    stitcher_ = Stitcher();
    lastGrab_ = {};
    frames_ = 0;

    autoMode_ = false;
    awaitingStable_ = false;
    driver_ = Driver::Wheel;
    noNewStreak_ = 0;
    failStreak_ = 0;
    prevTickFrame_ = {};

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
    connect(bar_, &ScrollCaptureBar::autoToggled, this, [this](bool enabled) {
        autoMode_ = enabled;
        awaitingStable_ = false;
        sinceInjectMs_ = kAutoStepIntervalMs; // 开关切换后立即可注入
        driver_ = Driver::Wheel;
        noNewStreak_ = 0;
        failStreak_ = 0;
        spdlog::info("scroll capture auto mode: {}", enabled ? "on" : "off");
        if (bar_) {
            bar_->setStatus(enabled ? QStringLiteral("自动滚动中…")
                                    : QStringLiteral("滚动目标窗口继续拼接…"));
        }
    });
    bar_->show();

    timer_.start();
    spdlog::info("scroll capture started: region {}x{} on screen dpr {}",
                 regionGlobal_.width(), regionGlobal_.height(),
                 screen_->devicePixelRatio());
}

void ScrollCaptureService::tick() {
    const QRect local = regionGlobal_.translated(-screen_->geometry().topLeft());
    const QImage frame = capturer_.grabScreenRegion(screen_, local);
    if (frame.isNull()) {
        return;
    }

    if (!stitcher_.active()) {
        stitcher_.begin(frame);
        frames_ = 1;
        lastGrab_ = frame;
        recordFrame(frame);
        bar_->setStatus(QStringLiteral("已捕获首帧,滚动目标窗口…"));
        if (autoMode_ && injector_) {
            injectStep();
            awaitingStable_ = true;
            stableWaitMs_ = 0;
            sinceInjectMs_ = 0;
            prevTickFrame_ = frame;
        }
        return;
    }

    if (autoMode_ && injector_) {
        tickAuto(frame);
        return;
    }

    // 手动模式:画面有变化才喂拼接器
    if (!lastGrab_.isNull() && frame == lastGrab_) {
        return;
    }
    lastGrab_ = frame;
    recordFrame(frame);
    handleAppend(stitcher_.append(frame));
}

void ScrollCaptureService::recordFrame(const QImage& frame) {
    if (recordDir_.isEmpty()) {
        return;
    }
    frame.save(QDir(recordDir_).filePath(
        QStringLiteral("frame_%1.png").arg(recordIndex_++, 4, 10, QLatin1Char('0'))));
}

// 自动模式:注入滚轮 → 等画面稳定(连续两帧一致或超时)→ 拼接 →
// 满足最小步间隔后再注入下一步
void ScrollCaptureService::tickAuto(const QImage& frame) {
    sinceInjectMs_ += kFrameIntervalMs;

    if (awaitingStable_) {
        const bool stable = (frame == prevTickFrame_);
        stableWaitMs_ += kFrameIntervalMs;
        prevTickFrame_ = frame;
        if (!stable && stableWaitMs_ < kStableTimeoutMs) {
            return; // 滚动动画/懒加载未完成
        }
        awaitingStable_ = false;
        recordFrame(frame);
        handleAppend(stitcher_.append(frame));
        if (!timer_.isActive()) {
            return; // handleAppend 内部可能已自动完成
        }
    }
    if (sinceInjectMs_ < kAutoStepIntervalMs) {
        return; // 节奏限制,避免滚动过快
    }
    injectStep();
    awaitingStable_ = true;
    stableWaitMs_ = 0;
    sinceInjectMs_ = 0;
    prevTickFrame_ = frame;
}

void ScrollCaptureService::injectStep() {
    if (driver_ == Driver::Wheel) {
        injector_->sendScroll(regionGlobal_.center(),
                              autoScrollDelta(regionGlobal_.height()));
    } else {
        injector_->sendKey(regionGlobal_.center(), Qt::Key_PageDown);
    }
}

void ScrollCaptureService::handleAppend(Stitcher::AppendResult result) {
    switch (result) {
    case Stitcher::AppendResult::Appended: {
        ++frames_;
        noNewStreak_ = 0;
        failStreak_ = 0;
        const int logicalHeight =
            qRound(stitcher_.resultHeight() / screen_->devicePixelRatio());
        bar_->setStatus(QStringLiteral("已拼接 %1 px(%2 帧)%3")
                            .arg(logicalHeight)
                            .arg(frames_)
                            .arg(autoMode_ ? QStringLiteral(",自动滚动中…")
                                           : QStringLiteral(",完成后点[完成]")));
        break;
    }
    case Stitcher::AppendResult::NoNewContent:
        if (!autoMode_) {
            break;
        }
        ++noNewStreak_;
        // 起步阶段滚轮注入毫无位移 → 该应用不吃滚轮消息,切 PageDown 驱动
        if (driver_ == Driver::Wheel && frames_ <= 1 && noNewStreak_ >= 2) {
            driver_ = Driver::PageDown;
            noNewStreak_ = 0;
            spdlog::info("scroll capture: wheel ineffective, switching to PageDown");
            bar_->setStatus(QStringLiteral("滚轮无效,已改用 PageDown 驱动…"));
            break;
        }
        if (noNewStreak_ >= kAutoFinishStreak) {
            spdlog::info("scroll capture: bottom reached, auto-finishing");
            finishCapture();
        }
        break;
    case Stitcher::AppendResult::MatchFailed:
        if (autoMode_) {
            if (++failStreak_ >= kAutoFailStreak) {
                autoMode_ = false;
                bar_->setAutoChecked(false);
                bar_->setStatus(
                    QStringLiteral("自动滚动对齐失败,已切回手动,请手动滚动"));
            }
        } else {
            bar_->setStatus(QStringLiteral("未能对齐:请往回滚动少许,放慢速度"));
        }
        break;
    }
}

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
    case Outlet::Copy:
        output_.copyToClipboard(result);
        emit copiedToClipboard(qRound(result.height() / dpr));
        break;
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
    lastGrab_ = {};
}

} // namespace pixora
