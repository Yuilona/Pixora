#pragma once

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

namespace pixora {

// 托盘图标与托盘菜单。M0 仅提供版本信息与退出入口,
// 后续里程碑在此挂截图/贴图/长截图/设置等动作。
class TrayService : public QObject {
    Q_OBJECT
public:
    explicit TrayService(QObject* parent = nullptr);

    void show();
    void notify(const QString& title, const QString& message);

signals:
    void captureRequested();

private:
    QSystemTrayIcon tray_;
    QMenu menu_;
};

} // namespace pixora
