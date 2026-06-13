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
        case HotkeyId::PinFromClipboard:
            emit pinRequested();
            break;
        case HotkeyId::RepeatLastRegion:
            emit repeatLastRegionRequested();
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
    failed_.clear();
    if (!backend_) {
        spdlog::info("global hotkeys not available on this platform");
        return;
    }
    const struct {
        HotkeyId id;
        QKeySequence seq;
        const char* name;
        QString action;
    } entries[] = {
        {HotkeyId::CaptureRegion, settings_.hotkeyCaptureRegion(), "capture",
         tr("capture")},
        {HotkeyId::PinFromClipboard, settings_.hotkeyPinFromClipboard(), "pin",
         tr("pin")},
        {HotkeyId::RepeatLastRegion, settings_.hotkeyRepeatLastRegion(),
         "repeat-region", tr("repeat last region")},
    };
    for (const auto& e : entries) {
        if (e.seq.isEmpty()) {
            // 空序列 = 用户未绑定该热键(如默认的"重做上次选区"),静默跳过
            spdlog::info("hotkey {} not bound, skipped", e.name);
            continue;
        }
        const bool ok = backend_->registerHotkey(e.id, e.seq);
        spdlog::info("hotkey {} ({}) -> {}", e.name,
                     e.seq.toString(QKeySequence::PortableText).toStdString(),
                     ok ? "registered" : "FAILED");
        if (!ok) {
            failed_.insert(static_cast<int>(e.id));
            emit registrationFailed(e.action, e.seq);
        }
    }
}

} // namespace pixora
