#include "app/CaptureService.h"
#include "app/HistoryService.h"
#include "app/HotkeyService.h"
#include "app/PinService.h"
#include "app/ScreenTextService.h"
#include "app/ScrollCaptureService.h"
#include "app/SettingsService.h"
#include "app/SingleInstanceGuard.h"
#include "app/TrayService.h"
#include "app/UpdateChecker.h"
#include "common/Log.h"
#include "ui/history/HistoryWindow.h"
#include "ui/settings/SettingsDialog.h"
#include "ui/Theme.h"
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
#include <QLocale>
#include <QPointer>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Pixora"));
    QApplication::setApplicationName(QStringLiteral("Pixora"));
    QApplication::setApplicationVersion(QStringLiteral(PIXORA_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/pixora-256.png")));
    app.setQuitOnLastWindowClosed(false);
    // 统一 UI 风格:文档型窗口(设置/历史/菜单)走 Fusion + 白色系样式表,
    // 与系统浅色标题栏衔接;悬浮 HUD(工具栏/通知卡)保持暗色(见 ui/Theme.h)
    QApplication::setStyle(new pixora::theme::DropDownStyle(
        QStyleFactory::create(QStringLiteral("Fusion"))));
    QPalette palette = QApplication::palette();
    palette.setColor(QPalette::Highlight, pixora::theme::accent());
    palette.setColor(QPalette::HighlightedText, Qt::white);
    QApplication::setPalette(palette);
    app.setStyleSheet(pixora::theme::appStyleSheet());

    pixora::initLogging();
    spdlog::info("Pixora {} starting", PIXORA_VERSION);

    // 语言:源码字符串为英文,中文经 :/i18n 里的 qm 翻译;
    // auto = 跟随系统(中文环境加载 zh_CN,其它回退英文)
    pixora::SettingsService settings;
    spdlog::info("settings file: {}", settings.filePath().toStdString());
    QTranslator translator;
    auto applyLanguage = [&app, &translator](const QString& mode) {
        QApplication::removeTranslator(&translator);
        bool loaded = false;
        if (mode == QLatin1String("auto")) {
            loaded = translator.load(QLocale(), QStringLiteral("pixora"),
                                     QStringLiteral("_"), QStringLiteral(":/i18n"));
        } else if (mode != QLatin1String("en")) {
            loaded = translator.load(QStringLiteral("pixora_") + mode,
                                     QStringLiteral(":/i18n"));
        }
        if (loaded) {
            QApplication::installTranslator(&translator);
        }
        spdlog::info("language mode '{}', translation {}", mode.toStdString(),
                     loaded ? "loaded" : "not loaded (English)");
    };
    applyLanguage(settings.language());

    const bool crashedLastRun = pixora::installCrashHandler(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        QStringLiteral("/crashes"));
    if (app.arguments().contains(QStringLiteral("--crash-test"))) {
        // 隐藏参数:故意空指针解引用,用于验证崩溃捕获链路
        spdlog::warn("--crash-test: triggering deliberate crash");
        volatile int* p = nullptr;
        *p = 42;
    }
    if (app.arguments().contains(QStringLiteral("--show-settings"))) {
        // 隐藏参数:独立打开设置窗口,主题/布局调试用(可与主实例并存)
        const auto devSystem = pixora::createSystemIntegration();
        auto* dialog = new pixora::SettingsDialog(settings, devSystem.get());
        QObject::connect(dialog, &QDialog::finished, &app, &QApplication::quit);
        dialog->show();
        return QApplication::exec();
    }

    pixora::SingleInstanceGuard guard(QStringLiteral("pixora-single-instance"));
    if (!guard.tryAcquire()) {
        spdlog::info("another instance is already running, exiting");
        return 0;
    }
    QObject::connect(&guard, &pixora::SingleInstanceGuard::secondInstanceStarted, [] {
        spdlog::info("second instance attempted to start");
    });

    pixora::TrayService tray;
    tray.show();
    // 语言切换即时生效:换翻译器 + 重建托盘菜单(已开窗口重开后生效)
    QObject::connect(&settings, &pixora::SettingsService::changed, &tray,
                     [&settings, &tray, &applyLanguage](const QString& key) {
                         if (key == QLatin1String("ui/language")) {
                             applyLanguage(settings.language());
                             tray.retranslate();
                         }
                     });
    if (crashedLastRun) {
        tray.notify(
            QCoreApplication::translate("main", "Pixora exited abnormally last time"),
            QCoreApplication::translate(
                "main", "A diagnostic file was written; please attach the logs and "
                        "the crashes folder when reporting the issue"));
    }

    const auto screenCapturer = pixora::createScreenCapturer();
    const auto windowEnumerator = pixora::createWindowEnumerator();
    const auto elementLocator = pixora::createElementLocator();
    pixora::HistoryService history(&settings);
    pixora::CaptureService capture(*screenCapturer, windowEnumerator.get(),
                                   elementLocator.get(), &settings, &history);
    QObject::connect(&capture, &pixora::CaptureService::copiedToClipboard, &tray, [&tray] {
        tray.notify(QStringLiteral("Pixora"),
                    QCoreApplication::translate("main", "Screenshot copied to clipboard"));
    });
    QObject::connect(&capture, &pixora::CaptureService::savedToFile, &tray,
                     [&tray](const QString& path) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QCoreApplication::translate(
                                         "main", "Screenshot saved: %1")
                                         .arg(path));
                     });
    QObject::connect(&tray, &pixora::TrayService::captureRequested, &capture,
                     [&capture] { capture.start(); });
    QObject::connect(&tray, &pixora::TrayService::colorPickRequested, &capture,
                     [&capture] { capture.startColorPickOnly(); });

    const auto systemIntegration = pixora::createSystemIntegration();
    pixora::PinService pins(systemIntegration.get(), &settings);
    QObject::connect(&capture, &pixora::CaptureService::pinCaptured, &pins,
                     &pixora::PinService::pinImage);
    QObject::connect(&tray, &pixora::TrayService::closeAllPinsRequested, &pins,
                     &pixora::PinService::closeAll);
    QObject::connect(&capture, &pixora::CaptureService::colorCopied, &tray,
                     [&tray](const QString& text) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QCoreApplication::translate(
                                         "main", "Color %1 copied")
                                         .arg(text));
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
    // 重做上次选区:以缓存的上次选区直接发起截图(默认不绑定热键)
    QObject::connect(&hotkeys, &pixora::HotkeyService::repeatLastRegionRequested,
                     &capture, [&capture, &scrollCapture] {
                         spdlog::info("hotkey: repeat last region requested");
                         if (scrollCapture.isActive()) {
                             return;
                         }
                         capture.startRepeatLastRegion();
                     });
    // 工具栏[长截图]:截图选区移交给滚动拼接
    QObject::connect(&capture, &pixora::CaptureService::scrollCaptureRequested,
                     &scrollCapture,
                     [&scrollCapture](const QRect& region) { scrollCapture.start(region); });
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::copiedToClipboard,
                     &tray, [&tray](int height) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QCoreApplication::translate(
                                         "main", "Scrolling capture copied (%1 px tall)")
                                         .arg(height));
                     });
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::pinCaptured, &pins,
                     &pixora::PinService::pinImage);
    QObject::connect(&scrollCapture, &pixora::ScrollCaptureService::savedToFile, &tray,
                     [&tray](const QString& path) {
                         tray.notify(QStringLiteral("Pixora"),
                                     QCoreApplication::translate(
                                         "main", "Scrolling capture saved: %1")
                                         .arg(path));
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
                                     QCoreApplication::translate(
                                         "main", "Recognized text copied (%1 lines)")
                                         .arg(lineCount));
                     });
    QObject::connect(&textService, &pixora::ScreenTextService::failed, &tray,
                     [&tray](const QString& reason) {
                         tray.notify(QCoreApplication::translate(
                                         "main", "OCR / Translate"),
                                     reason);
                     });

    QObject::connect(&hotkeys, &pixora::HotkeyService::pinRequested, &pins,
                     [&pins, &tray] {
                         spdlog::info("hotkey: pin requested");
                         if (!pins.pinFromClipboard()) {
                             tray.notify(QStringLiteral("Pixora"),
                                         QCoreApplication::translate(
                                             "main", "No image in the clipboard"));
                         }
                     });
    QObject::connect(&hotkeys, &pixora::HotkeyService::registrationFailed, &tray,
                     [&tray](const QString& action, const QKeySequence& seq) {
                         tray.notify(
                             QCoreApplication::translate("main",
                                                         "Hotkey registration failed"),
                             QCoreApplication::translate(
                                 "main",
                                 "The %1 hotkey %2 may be taken by another program; "
                                 "change it in tray menu > Settings")
                                 .arg(action, seq.toString(QKeySequence::NativeText)));
                     });
    hotkeys.registerAll();

    // 更新检查:启动 5s 后拉 GitHub Releases(不挡启动路径),
    // 有新版弹通知卡,点击打开下载页;设置可关
    pixora::UpdateChecker updateChecker(&settings);
    QObject::connect(
        &updateChecker, &pixora::UpdateChecker::updateAvailable, &tray,
        [&tray](const QString& version, const QString& url) {
            tray.notify(QCoreApplication::translate("main", "Update available"),
                        QCoreApplication::translate(
                            "main", "Pixora %1 is out (you have %2) - click to "
                                    "open the download page")
                            .arg(version, QApplication::applicationVersion()),
                        url);
        });
    QTimer::singleShot(5000, &updateChecker,
                       &pixora::UpdateChecker::checkOnStartup);

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
                             hotkeys.failed(pixora::HotkeyId::PinFromClipboard),
                             hotkeys.failed(pixora::HotkeyId::RepeatLastRegion));
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
