#include "app/CaptureService.h"
#include "app/HotkeyService.h"
#include "app/SettingsService.h"
#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "common/Log.h"
#include "platform/interface/PlatformFactory.h"
#include "platform/interface/ScreenCapturer.h"

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
    pixora::CaptureService capture(*screenCapturer);
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

    const auto hotkeyBackend = pixora::createGlobalHotkey();
    pixora::HotkeyService hotkeys(settings, hotkeyBackend.get());

    QObject::connect(&hotkeys, &pixora::HotkeyService::captureRequested, &capture,
                     [&capture] {
                         spdlog::info("hotkey: capture requested");
                         capture.start();
                     });
    QObject::connect(&hotkeys, &pixora::HotkeyService::scrollCaptureRequested, &tray, [&tray] {
        spdlog::info("hotkey: scroll capture requested");
        tray.notify(QStringLiteral("Pixora"), QStringLiteral("长截图热键已触发(M3 实现长截图)"));
    });
    QObject::connect(&hotkeys, &pixora::HotkeyService::pinRequested, &tray, [&tray] {
        spdlog::info("hotkey: pin requested");
        tray.notify(QStringLiteral("Pixora"), QStringLiteral("贴图热键已触发(M2 实现贴图)"));
    });
    hotkeys.registerAll();

    const int rc = QApplication::exec();
    spdlog::info("Pixora exiting with code {}", rc);
    return rc;
}
