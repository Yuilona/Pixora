#include "app/CaptureService.h"

#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
#include "platform/interface/ScreenCapturer.h"
#include "ui/overlay/OverlayWindow.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>

#include <spdlog/spdlog.h>

namespace pixora {

CaptureService::CaptureService(IScreenCapturer& capturer, QObject* parent)
    : QObject(parent), capturer_(capturer) {}

CaptureService::~CaptureService() {
    teardown();
}

void CaptureService::start() {
    if (isActive()) {
        spdlog::debug("snip session already active, ignoring trigger");
        return;
    }

    QElapsedTimer timer;
    timer.start();
    std::vector<ScreenSnap> snaps;
    for (QScreen* screen : QGuiApplication::screens()) {
        snaps.push_back(ScreenSnap{capturer_.grabScreen(screen), screen->geometry(),
                                   screen->devicePixelRatio()});
    }
    session_ = std::make_unique<SnipSession>(DesktopSnapshot(std::move(snaps)));
    spdlog::info("snip session started: {} screen(s) frozen in {} ms",
                 session_->snapshot().screens().size(), timer.elapsed());

    connect(session_.get(), &SnipSession::confirmed, this, [this](const QRect& region) {
        const QImage image = session_->snapshot().copyRegionLogical(region);
        teardown();
        output_.copyToClipboard(image);
        emit copiedToClipboard();
    });
    connect(session_.get(), &SnipSession::saveRequested, this,
            [this](const QRect& region) {
                const QImage image = session_->snapshot().copyRegionLogical(region);
                teardown();
                const QString path = output_.saveWithDialog(image);
                if (!path.isEmpty()) {
                    emit savedToFile(path);
                }
            });
    connect(session_.get(), &SnipSession::cancelled, this, [this] {
        spdlog::info("snip session cancelled");
        teardown();
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

void CaptureService::teardown() {
    for (OverlayWindow* overlay : overlays_) {
        overlay->close(); // WA_DeleteOnClose
    }
    overlays_.clear();
    if (session_) {
        session_->disconnect(this);
        session_.release()->deleteLater();
    }
}

} // namespace pixora
