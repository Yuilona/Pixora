#include "app/ScrollCaptureService.h"

#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
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
constexpr int kFrameIntervalMs = 66; // ~15fps
} // namespace

ScrollCaptureService::ScrollCaptureService(IScreenCapturer& capturer,
                                           IWindowEnumerator* enumerator,
                                           QObject* parent)
    : QObject(parent), capturer_(capturer), enumerator_(enumerator) {
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

    indicator_ = new RegionIndicator(regionGlobal_);
    indicator_->show();
    bar_ = new ScrollCaptureBar(regionGlobal_, screen_->virtualGeometry());
    connect(bar_, &ScrollCaptureBar::finishRequested, this,
            &ScrollCaptureService::finishCapture);
    connect(bar_, &ScrollCaptureBar::cancelRequested, this, [this] {
        spdlog::info("scroll capture cancelled");
        teardownScroll();
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
    if (!lastGrab_.isNull() && frame == lastGrab_) {
        return; // 画面静止,跳过
    }
    lastGrab_ = frame;

    if (!stitcher_.active()) {
        stitcher_.begin(frame);
        frames_ = 1;
        bar_->setStatus(QStringLiteral("已捕获首帧,滚动目标窗口…"));
        return;
    }
    switch (stitcher_.append(frame)) {
    case Stitcher::AppendResult::Appended: {
        ++frames_;
        const int logicalHeight =
            qRound(stitcher_.resultHeight() / screen_->devicePixelRatio());
        bar_->setStatus(QStringLiteral("已拼接 %1 px(%2 帧),完成后点[完成]")
                            .arg(logicalHeight)
                            .arg(frames_));
        break;
    }
    case Stitcher::AppendResult::NoNewContent:
        break;
    case Stitcher::AppendResult::MatchFailed:
        bar_->setStatus(QStringLiteral("未能对齐:请往回滚动少许,放慢速度"));
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
