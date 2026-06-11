#include "app/ScrollCaptureService.h"

#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/ScreenCapturer.h"
#include "platform/interface/WindowEnumerator.h"
#include "ui/overlay/OverlayWindow.h"
#include "ui/scroll/RegionIndicator.h"
#include "ui/scroll/ScrollCaptureBar.h"

#include <QGuiApplication>
#include <QScreen>

#include <spdlog/spdlog.h>

namespace pixora {

namespace {
constexpr int kFrameIntervalMs = 66;     // ~15fps
constexpr int kAutoScrollDelta = -360;   // 每步 3 格滚轮
constexpr int kStableTimeoutMs = 800;    // 稳定等待超时(懒加载兜底)
constexpr int kAutoFinishStreak = 3;     // 连续无新内容 → 判定到底
constexpr int kAutoFailStreak = 3;       // 连续匹配失败 → 退回手动
} // namespace

ScrollCaptureService::ScrollCaptureService(IScreenCapturer& capturer,
                                           IWindowEnumerator* enumerator,
                                           IInputInjector* injector, QObject* parent)
    : QObject(parent), capturer_(capturer), enumerator_(enumerator),
      injector_(injector) {
    timer_.setInterval(kFrameIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &ScrollCaptureService::tick);
}

ScrollCaptureService::~ScrollCaptureService() {
    teardownSelection();
    teardownScroll();
}

void ScrollCaptureService::start() {
    if (timer_.isActive()) {
        finishCapture(); // 再按一次 F2 = 完成
        return;
    }
    if (session_) {
        return; // 选区阶段进行中
    }

    std::vector<ScreenSnap> snaps;
    for (QScreen* screen : QGuiApplication::screens()) {
        snaps.push_back(ScreenSnap{capturer_.grabScreen(screen), screen->geometry(),
                                   screen->devicePixelRatio()});
    }
    session_ = std::make_unique<SnipSession>(DesktopSnapshot(std::move(snaps)));
    if (enumerator_) {
        session_->setWindowCandidates(enumerator_->topLevelWindows());
    }
    spdlog::info("scroll capture: selecting region");

    connect(session_.get(), &SnipSession::confirmed, this, [this](const QRect& region) {
        teardownSelection();
        beginScrollPhase(region);
    });
    connect(session_.get(), &SnipSession::cancelled, this, [this] {
        spdlog::info("scroll capture cancelled at selection phase");
        teardownSelection();
    });

    for (const ScreenSnap& snap : session_->snapshot().screens()) {
        auto* overlay = new OverlayWindow(snap, *session_);
        overlays_.push_back(overlay);
        overlay->show();
    }
    if (!overlays_.empty()) {
        overlays_.front()->activateWindow();
        overlays_.front()->raise();
    }
}

void ScrollCaptureService::beginScrollPhase(QRect regionGlobal) {
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
    noNewStreak_ = 0;
    failStreak_ = 0;
    prevTickFrame_ = {};

    indicator_ = new RegionIndicator(regionGlobal_);
    indicator_->show();
    bar_ = new ScrollCaptureBar(regionGlobal_, screen_->virtualGeometry(),
                                injector_ != nullptr);
    connect(bar_, &ScrollCaptureBar::finishRequested, this,
            &ScrollCaptureService::finishCapture);
    connect(bar_, &ScrollCaptureBar::cancelRequested, this, [this] {
        spdlog::info("scroll capture cancelled");
        teardownScroll();
    });
    connect(bar_, &ScrollCaptureBar::autoToggled, this, [this](bool enabled) {
        autoMode_ = enabled;
        awaitingStable_ = false;
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
        bar_->setStatus(QStringLiteral("已捕获首帧,滚动目标窗口…"));
        if (autoMode_ && injector_) {
            injector_->sendScroll(regionGlobal_.center(), kAutoScrollDelta);
            awaitingStable_ = true;
            stableWaitMs_ = 0;
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
    handleAppend(stitcher_.append(frame));
}

// 自动模式:注入滚轮后等待画面稳定(连续两帧一致或超时),再拼接下一步
void ScrollCaptureService::tickAuto(const QImage& frame) {
    if (awaitingStable_) {
        const bool stable = (frame == prevTickFrame_);
        stableWaitMs_ += kFrameIntervalMs;
        prevTickFrame_ = frame;
        if (!stable && stableWaitMs_ < kStableTimeoutMs) {
            return; // 滚动动画/懒加载未完成
        }
        awaitingStable_ = false;
        handleAppend(stitcher_.append(frame));
        if (!timer_.isActive()) {
            return; // handleAppend 内部可能已自动完成
        }
    }
    injector_->sendScroll(regionGlobal_.center(), kAutoScrollDelta);
    awaitingStable_ = true;
    stableWaitMs_ = 0;
    prevTickFrame_ = frame;
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
        if (autoMode_ && ++noNewStreak_ >= kAutoFinishStreak) {
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

void ScrollCaptureService::finishCapture() {
    timer_.stop();
    QImage result = stitcher_.result();
    const qreal dpr = screen_ ? screen_->devicePixelRatio() : 1.0;
    teardownScroll();
    if (result.isNull()) {
        spdlog::info("scroll capture finished with no content");
        return;
    }
    result.setDevicePixelRatio(dpr);
    output_.copyToClipboard(result);
    emit copiedToClipboard(qRound(result.height() / dpr));
    spdlog::info("scroll capture finished: {}x{} px, {} frames", result.width(),
                 result.height(), frames_);
}

void ScrollCaptureService::teardownSelection() {
    for (OverlayWindow* overlay : overlays_) {
        overlay->close();
    }
    overlays_.clear();
    if (session_) {
        session_->disconnect(this);
        session_.release()->deleteLater();
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
