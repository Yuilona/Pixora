#include "app/TrayService.h"

#include <QApplication>
#include <QCoreApplication>

#include <spdlog/spdlog.h>

namespace pixora {

TrayService::TrayService(QObject* parent) : QObject(parent) {
    auto* title = menu_.addAction(
        QStringLiteral("Pixora %1").arg(QApplication::applicationVersion()));
    title->setEnabled(false);
    menu_.addSeparator();
    menu_.addAction(QStringLiteral("截图\tF1"), this, [this] { emit captureRequested(); });
    menu_.addAction(QStringLiteral("关闭所有贴图"), this,
                    [this] { emit closeAllPinsRequested(); });
    menu_.addSeparator();
    menu_.addAction(QStringLiteral("设置…"), this, [this] { emit settingsRequested(); });
    menu_.addSeparator();
    menu_.addAction(QStringLiteral("退出"), [] { QCoreApplication::quit(); });

    tray_.setIcon(QIcon(QStringLiteral(":/icons/pixora-256.png")));
    tray_.setToolTip(QStringLiteral("Pixora"));
    tray_.setContextMenu(&menu_);
}

void TrayService::notify(const QString& title, const QString& message) {
    tray_.showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void TrayService::show() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        spdlog::error("system tray is not available on this desktop environment");
        return;
    }
    tray_.show();
    spdlog::info("tray icon shown");
}

} // namespace pixora
