#include "app/TrayService.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStyle>

#include <spdlog/spdlog.h>

namespace pixora {

TrayService::TrayService(QObject* parent) : QObject(parent) {
    auto* title = menu_.addAction(
        QStringLiteral("Pixora %1 (M0)").arg(QApplication::applicationVersion()));
    title->setEnabled(false);
    menu_.addSeparator();
    menu_.addAction(QStringLiteral("退出"), [] { QCoreApplication::quit(); });

    // M0 暂用系统标准图标,正式图标随 resources/ 在后续里程碑加入。
    tray_.setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
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
