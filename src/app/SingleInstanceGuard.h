#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

namespace pixora {

// 基于 QLocalServer 的单实例守卫:第一个实例监听本地套接字,
// 后续实例连接成功即知已有实例在运行,通知后自行退出。
class SingleInstanceGuard : public QObject {
    Q_OBJECT
public:
    explicit SingleInstanceGuard(QString key, QObject* parent = nullptr);

    // 返回 true 表示本进程是第一个实例(已开始监听);
    // 返回 false 表示已有实例在运行(已向其发送唤起通知)。
    bool tryAcquire();

signals:
    // 有第二个实例尝试启动(后续里程碑用于唤起主界面)。
    void secondInstanceStarted();

private:
    QString key_;
    QLocalServer server_;
};

} // namespace pixora
