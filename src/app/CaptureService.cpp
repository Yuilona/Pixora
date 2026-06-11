#include "app/CaptureService.h"

#include "app/HistoryService.h"
#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
#include "platform/interface/ScreenCapturer.h"
#include "platform/interface/WindowEnumerator.h"
#include "ui/editor/AnnotationRenderer.h"
#include "ui/editor/AnnotationToolbar.h"
#include "ui/overlay/OverlayWindow.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QScreen>

#include <spdlog/spdlog.h>

namespace pixora {

CaptureService::CaptureService(IScreenCapturer& capturer, IWindowEnumerator* enumerator,
                               const SettingsService* settings, HistoryService* history,
                               QObject* parent)
    : QObject(parent), capturer_(capturer), enumerator_(enumerator), output_(settings),
      history_(history) {}

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

    if (enumerator_) {
        auto candidates = enumerator_->topLevelWindows();
        spdlog::debug("window snap candidates: {}", candidates.size());
        session_->setWindowCandidates(std::move(candidates));
    }

    connect(session_.get(), &SnipSession::confirmed, this, [this](const QRect& region) {
        const QImage image = renderAndRecord(region);
        teardown();
        output_.copyToClipboard(image);
        emit copiedToClipboard();
    });
    connect(session_.get(), &SnipSession::saveRequested, this,
            [this](const QRect& region) {
                const QImage image = renderAndRecord(region);
                teardown();
                const QString path = output_.saveWithDialog(image);
                if (!path.isEmpty()) {
                    emit savedToFile(path);
                }
            });
    connect(session_.get(), &SnipSession::pinRequested, this,
            [this](const QRect& region) {
                const QImage image = renderAndRecord(region);
                const QPoint topLeft = region.topLeft();
                teardown();
                emit pinCaptured(image, topLeft); // 贴图出现在原选区位置
            });
    connect(session_.get(), &SnipSession::scrollRequested, this,
            [this](const QRect& region) {
                teardown(); // 先拆遮罩,让目标窗口可滚动
                emit scrollCaptureRequested(region);
            });
    connect(session_.get(), &SnipSession::colorPicked, this, [this](const QString& text) {
        teardown();
        QGuiApplication::clipboard()->setText(text);
        emit colorCopied(text);
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
    toolbar_ = new AnnotationToolbar(*session_); // 选区交互结束后自行显示
}

QImage CaptureService::renderResult(const QRect& region) const {
    QImage image = session_->snapshot().copyRegionLogical(region);
    return AnnotationRenderer::flatten(std::move(image), session_->document(),
                                       region.topLeft(), session_->snapshot());
}

QImage CaptureService::renderAndRecord(const QRect& region) {
    QImage image = renderResult(region);
    if (history_) {
        history_->record(image);
    }
    return image;
}

void CaptureService::teardown() {
    for (OverlayWindow* overlay : overlays_) {
        overlay->close(); // WA_DeleteOnClose
    }
    overlays_.clear();
    if (toolbar_) {
        toolbar_->close();
        toolbar_->deleteLater();
        toolbar_ = nullptr;
    }
    if (session_) {
        session_->disconnect(this);
        session_.release()->deleteLater();
    }
}

} // namespace pixora
