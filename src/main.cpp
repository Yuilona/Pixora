#include "app/CaptureService.h"
#include "app/HistoryService.h"
#include "app/HotkeyService.h"
#include "app/PinService.h"
#include "app/ScreenTextService.h"
#include "app/ScrollCaptureService.h"
#include "app/SettingsService.h"
#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "common/Log.h"
#include "ui/history/HistoryWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "platform/interface/CrashHandler.h"
#include "platform/interface/ElementLocator.h"
#include "platform/interface/InputInjector.h"
#include "platform/interface/PlatformFactory.h"
#include "platform/interface/ScreenCapturer.h"
#include "platform/interface/SystemIntegration.h"
#include "platform/interface/WindowEnumerator.h"

#include <QApplication>
#include <QCursor>
#include <QIcon>
#include <QPointer>
#include <QStandardPaths>

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

    const bool crashedLastRun = pixora::installCrashHandler(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        QStringLiteral("/crashes"));
    if (app.arguments().contains(QStringLiteral("--crash-test"))) {
        // 隐藏参数:故意空指针解引用,用于验证崩溃捕获链路
        spdlog::warn("--crash-test: triggering deliberate crash");
        volatile int* p = nullptr;
        *p = 42;
    }

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
    if (crashedLastRun) {
        tray.notify(QStringLiteral("Pixora 上次异常退出"),
                    QStringLiteral("已生成诊断文件;反馈问题时请附上日志与 crashes 目录"));
    }

    const auto screenCapturer = pixora::createScreenCapturer();
    const auto windowEnumerator = pixora::createWindowEnumerator();
    const auto elementLocator = pixora::createElementLocator();
    pixora::HistoryService history(&settings);
    pixora::CaptureService capture(*screenCapturer, windowEnumerator.get(),
                                   elementLocator.get(), &settings, &history);
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
    QObject::connect(&capture, &pixora::CaptureService::colorCopied, &tray,
                     [&tray](const QString& text) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("已复制颜色 %1").arg(text));
                     });
    pins.restorePins(); // 恢复上次会话留下的贴图

    const auto hotkeyBackend = pixora::createGlobalHotkey();
    pixora::HotkeyService hotkeys(settings, hotkeyBackend.get());

    const auto inputInjector = pixora::createInputInjector();
    pixora::ScrollCaptureService scrollCapture(*screenCapturer, inputInjector.get(),
                                               &settings, &history);
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
    // 提取文字 / 截图翻译(无感替换为译文贴图)
    pixora::ScreenTextService textService(&settings, &pins);
    QObject::connect(&capture, &pixora::CaptureService::ocrRequested, &textService,
                     &pixora::ScreenTextService::extractText);
    QObject::connect(&capture, &pixora::CaptureService::translateRequested,
                     &textService, &pixora::ScreenTextService::translateInPlace);
    QObject::connect(&textService, &pixora::ScreenTextService::started, &tray,
                     [&tray](const QString& message) {
                         tray.notify(QStringLiteral("Pixora"), message);
                     });
    QObject::connect(&textService, &pixora::ScreenTextService::textCopied, &tray,
                     [&tray](int lineCount) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QStringLiteral("已复制识别文字(%1 行)")
                                         .arg(lineCount));
                     });
    QObject::connect(&textService, &pixora::ScreenTextService::failed, &tray,
                     [&tray](const QString& reason) {
                         tray.notify(QStringLiteral("OCR / 翻译"), reason);
                     });

    QObject::connect(&hotkeys, &pixora::HotkeyService::pinRequested, &pins,
                     [&pins, &tray] {
                         spdlog::info("hotkey: pin requested");
                         if (!pins.pinFromClipboard()) {
                             tray.notify(QStringLiteral("Pixora"),
                                         QStringLiteral("剪贴板中没有图像"));
                         }
                     });
    QObject::connect(&hotkeys, &pixora::HotkeyService::registrationFailed, &tray,
                     [&tray](const QString& action, const QKeySequence& seq) {
                         tray.notify(
                             QStringLiteral("热键注册失败"),
                             QStringLiteral("%1热键 %2 可能已被其它程序占用,"
                                            "请在托盘菜单 → 设置中更换")
                                 .arg(action, seq.toString(QKeySequence::NativeText)));
                     });
    hotkeys.registerAll();

    QPointer<pixora::HistoryWindow> historyWindow;
    QObject::connect(&tray, &pixora::TrayService::historyRequested, &tray,
                     [&history, &settings, &pins, &historyWindow] {
                         if (historyWindow) {
                             historyWindow->raise();
                             historyWindow->activateWindow();
                             return;
                         }
                         historyWindow = new pixora::HistoryWindow(history, &settings);
                         QObject::connect(historyWindow,
                                          &pixora::HistoryWindow::pinRequested, &pins,
                                          [&pins](const QImage& image) {
                                              pins.pinImage(image,
                                                            QCursor::pos() + QPoint(20, 20));
                                          });
                         historyWindow->show();
                         historyWindow->raise();
                         historyWindow->activateWindow();
                     });

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
                         settingsDialog->markHotkeyConflicts(
                             hotkeys.failed(pixora::HotkeyId::CaptureRegion),
                             hotkeys.failed(pixora::HotkeyId::PinFromClipboard));
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
