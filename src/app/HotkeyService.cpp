#include "app/HotkeyService.h"

#include "app/SettingsService.h"

#include <spdlog/spdlog.h>

namespace pixora {

HotkeyService::HotkeyService(SettingsService& settings, IGlobalHotkey* backend,
                             QObject* parent)
    : QObject(parent), settings_(settings), backend_(backend) {
    if (!backend_) {
        return;
    }
    connect(backend_, &IGlobalHotkey::activated, this, [this](HotkeyId id) {
        switch (id) {
        case HotkeyId::CaptureRegion:
            emit captureRequested();
            break;
        case HotkeyId::ScrollCapture:
            emit scrollCaptureRequested();
            break;
        case HotkeyId::PinFromClipboard:
            emit pinRequested();
            break;
        }
    });
}

void HotkeyService::reregisterAll() {
    if (backend_) {
        backend_->unregisterAll();
    }
    registerAll();
}

void HotkeyService::registerAll() {
    if (!backend_) {
        spdlog::info("global hotkeys not available on this platform");
        return;
    }
    const struct {
        HotkeyId id;
        QKeySequence seq;
        const char* name;
    } entries[] = {
        {HotkeyId::CaptureRegion, settings_.hotkeyCaptureRegion(), "capture"},
        {HotkeyId::ScrollCapture, settings_.hotkeyScrollCapture(), "scroll-capture"},
        {HotkeyId::PinFromClipboard, settings_.hotkeyPinFromClipboard(), "pin"},
    };
    for (const auto& e : entries) {
        const bool ok = backend_->registerHotkey(e.id, e.seq);
        spdlog::info("hotkey {} ({}) -> {}", e.name,
                     e.seq.toString(QKeySequence::PortableText).toStdString(),
                     ok ? "registered" : "FAILED");
    }
}

} // namespace pixora
