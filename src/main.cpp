#include "app/CaptureService.h"
#include "app/HotkeyService.h"
#include "app/PinService.h"
#include "app/ScrollCaptureService.h"
#include "app/SettingsService.h"
#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "common/Log.h"
#include "ui/settings/SettingsDialog.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/PlatformFactory.h"
#include "platform/interface/ScreenCapturer.h"
#include "platform/interface/SystemIntegration.h"
#include "platform/interface/WindowEnumerator.h"

#include <QApplication>
#include <QIcon>
#include <QPointer>

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Pixora"));
    QApplication::setApplicationName(QStringLiteral("Pixora"));
    QApplication::setApplicationVersion(QStringLiteral(PIXORA_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/pixora-256.png")));
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
    pixora::CaptureService capture(*screenCapturer, windowEnumerator.get(), &settings);
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

    const auto inputInjector = pixora::createInputInjector();
    pixora::ScrollCaptureService scrollCapture(*screenCapturer, inputInjector.get(),
                                               &settings);
    // 截图热键身兼两职:平时发起截图;长截图拼接中等同点[复制]完成
    QObject::connect(&hotkeys, &pixora::HotkeyService::captureRequested, &capture,
                     [&capture, &scrollCapture] {
                         spdlog::info("hotkey: capture requested");
                         if (scrollCapture.isActive()) {
                             scrollCapture.finish();
                             return;
                         }
                         capture.start();
                     });
    // 工具栏[长截图]:截图选区移交给滚动拼接
    QObject::connect(&capture, &pixora::CaptureService::scrollCaptureRequested,
                     &scrollCapture,
                     [&scrollCapture](const QRect& region) { scrollCapture.start(region); });
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::copiedToClipboard,
                     &tray, [&tray](int height) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("长截图已复制(高 %1 px)").arg(height));
                     });
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::pinCaptured, &pins,
                     &pixora::PinService::pinImage);
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::savedToFile, &tray,
                     [&tray](const QString& path) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("长截图已保存:%1").arg(path));
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

    QPointer<pixora::SettingsDialog> settingsDialog;
    QObject::connect(&tray, &pixora::TrayService::settingsRequested, &tray,
                     [&settings, &hotkeys, &systemIntegration, &settingsDialog] {
                         if (settingsDialog) {
                             settingsDialog->raise();
                             settingsDialog->activateWindow();
                             return;
                         }
                         settingsDialog = new pixora::SettingsDialog(
                             settings, systemIntegration.get());
                         QObject::connect(settingsDialog,
                                          &pixora::SettingsDialog::applied, &hotkeys,
                                          &pixora::HotkeyService::reregisterAll);
                         settingsDialog->show();
                         settingsDialog->raise();
                         settingsDialog->activateWindow();
                     });

    const int rc = QApplication::exec();
    spdlog::info("Pixora exiting with code {}", rc);
    return rc;
}
