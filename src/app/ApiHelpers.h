#pragma once

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

// OCR / 翻译等 HTTP 客户端共用的小工具。
namespace pixora::net {

// 末尾斜杠归一后拼接路径;base 已含该路径(用户填了完整地址)则原样返回
inline QString joinUrl(QString base, const QString& path) {
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (base.endsWith(path)) {
        return base;
    }
    return base + path;
}

// HTTP 失败的人话描述:状态码 + Qt 错误 + 响应体摘录(服务端报错常在体里)
inline QString httpFailureReason(QNetworkReply* reply) {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString excerpt = QString::fromUtf8(reply->readAll().left(300));
    return QStringLiteral("HTTP %1 %2: %3")
        .arg(status)
        .arg(reply->errorString(), excerpt);
}

} // namespace pixora::net
