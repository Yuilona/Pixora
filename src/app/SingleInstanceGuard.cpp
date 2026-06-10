#include "app/SingleInstanceGuard.h"

#include <QLocalSocket>

#include <spdlog/spdlog.h>

#include <utility>

namespace pixora {

SingleInstanceGuard::SingleInstanceGuard(QString key, QObject* parent)
    : QObject(parent), key_(std::move(key)) {}

bool SingleInstanceGuard::tryAcquire() {
    QLocalSocket probe;
    probe.connectToServer(key_);
    if (probe.waitForConnected(200)) {
        probe.write("activate");
        probe.waitForBytesWritten(200);
        return false;
    }

    // 上一个实例崩溃可能留下失效的套接字文件,先清理再监听。
    QLocalServer::removeServer(key_);
    if (!server_.listen(key_)) {
        spdlog::warn("single-instance server failed to listen: {}",
                     server_.errorString().toStdString());
        return true;
    }

    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* conn = server_.nextPendingConnection()) {
            conn->deleteLater();
        }
        emit secondInstanceStarted();
    });
    return true;
}

} // namespace pixora
