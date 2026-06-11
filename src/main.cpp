#include "app/CaptureService.h"
#include "app/HotkeyService.h"
#include "app/PinService.h"
#include "app/ScrollCaptureService.h"
#include "app/SettingsService.h"
#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "common/Log.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/PlatformFactory.h"
#include "platform/interface/ScreenCapturer.h"
#include "platform/interface/SystemIntegration.h"
#include "platform/interface/WindowEnumerator.h"

#include <QApplication>

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Pixora"));
    QApplication::setApplicationName(QStringLiteral("Pixora"));
    QApplication::setApplicationVersion(QStringLiteral(PIXORA_VERSION));
    app.setQuitOnLastWindowClosed(false);

    pixora::initLogging();
    spdlog::info("Pixora {} starting", PIXORA_VERSION);

    pixora::SingleInstanceGuard guard(QStringLiteral("pixora-single-instance"));
    if (!guard.tryAcquire()) {
        spdlog::info("another instance is already running, exiting");
        return 0;
    }
    QObject::connect(&guard, &pixora::SingleInstanceGuard::secondInstanceStarted, [] {
        spdlog::info("second instance attempted to start");
    });

    pixora::SettingsService settings;
    spdlog::info("settings file: {}", settings.filePath().toStdString());

    pixora::TrayService tray;
    tray.show();

    const auto screenCapturer = pixora::createScreenCapturer();
    const auto windowEnumerator = pixora::createWindowEnumerator();
    pixora::CaptureService capture(*screenCapturer, windowEnumerator.get());
    QObject::connect(&capture, &pixora::CaptureService::copiedToClipboard, &tray, [&tray] {
        tray.notify(QStringLiteral("Pixora"), QStringLiteral("截图已复制到剪贴板"));
    });
    QObject::connect(&capture, &pixora::CaptureService::savedToFile, &tray,
                     [&tray](const QString& path) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("截图已保存:%1").arg(path));
                     });
    QObject::connect(&tray, &pixora::TrayService::captureRequested, &capture,
                     [&capture] { capture.start(); });

    const auto systemIntegration = pixora::createSystemIntegration();
    pixora::PinService pins(systemIntegration.get());
    QObject::connect(&capture, &pixora::CaptureService::pinCaptured, &pins,
                     &pixora::PinService::pinImage);
    QObject::connect(&tray, &pixora::TrayService::closeAllPinsRequested, &pins,
                     &pixora::PinService::closeAll);

    const auto hotkeyBackend = pixora::createGlobalHotkey();
    pixora::HotkeyService hotkeys(settings, hotkeyBackend.get());

    QObject::connect(&hotkeys, &pixora::HotkeyService::captureRequested, &capture,
                     [&capture] {
                         spdlog::info("hotkey: capture requested");
                         capture.start();
                     });
    const auto inputInjector = pixora::createInputInjector();
    pixora::ScrollCaptureService scrollCapture(*screenCapturer, windowEnumerator.get(),
                                               inputInjector.get());
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::copiedToClipboard,
                     &tray, [&tray](int height) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("长截图已复制(高 %1 px)").arg(height));
                     });
    QObject::connect(&hotkeys, &pixora::HotkeyService::scrollCaptureRequested,
                     &scrollCapture, [&scrollCapture] {
                         spdlog::info("hotkey: scroll capture requested");
                         scrollCapture.start();
                     });
    QObject::connect(&hotkeys, &pixora::HotkeyService::pinRequested, &pins,
                     [&pins, &tray] {
                         spdlog::info("hotkey: pin requested");
                         if (!pins.pinFromClipboard()) {
                             tray.notify(QStringLiteral("Pixora"),
                                         QStringLiteral("剪贴板中没有图像"));
                         }
                     });
    hotkeys.registerAll();

    const int rc = QApplication::exec();
    spdlog::info("Pixora exiting with code {}", rc);
    return rc;
}
