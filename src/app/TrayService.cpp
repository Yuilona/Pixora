#include "app/TrayService.h"

#include "ui/notify/ToastWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QPixmap>
#include <QTimer>

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
    menu_.addAction(QStringLiteral("历史…"), this, [this] { emit historyRequested(); });
    menu_.addAction(QStringLiteral("设置…"), this, [this] { emit settingsRequested(); });
    menu_.addSeparator();
    menu_.addAction(QStringLiteral("退出"), [] { QCoreApplication::quit(); });

    // QPixmap 立即加载,能探测资源缺失(QIcon 懒加载,isNull 永远为假)
    const QPixmap pm(QStringLiteral(":/icons/pixora-256.png"));
    if (pm.isNull()) {
        spdlog::error("tray icon resource failed to load, using fallback");
        QPixmap fallback(32, 32);
        fallback.fill(theme::accent());
        tray_.setIcon(QIcon(fallback));
    } else {
        tray_.setIcon(QIcon(pm));
    }
    tray_.setToolTip(QStringLiteral("Pixora"));
    tray_.setContextMenu(&menu_);
}

TrayService::~TrayService() = default;

void TrayService::notify(const QString& title, const QString& message) {
    if (!toast_) {
        toast_ = std::make_unique<ToastWindow>();
    }
    toast_->popup(title, message);
}

void TrayService::show() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        spdlog::error("system tray is not available on this desktop environment");
        return;
    }
    tray_.show();
    spdlog::info("tray icon shown (visible: {})", tray_.isVisible());
    // 注册是异步落位的,延迟读几何:空几何 = Explorer 没接纳图标
    QTimer::singleShot(2000, this, [this] {
        const QRect g = tray_.geometry();
        spdlog::info("tray icon geometry after 2s: {}x{} at ({},{}), visible: {}",
                     g.width(), g.height(), g.x(), g.y(), tray_.isVisible());
    });
}

} // namespace pixora
