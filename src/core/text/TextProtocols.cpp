#include "core/text/TextProtocols.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <climits>

namespace pixora::textproto {

namespace {

// 错误文案用户可见(经 failed 信号进通知卡);自由函数无 tr(),
// 统一直呼 QCoreApplication::translate 挂在 "textproto" 上下文下——
// lupdate 看不穿自定义包装函数,经包装的字符串不会进 ts
void setError(QString* error, const QString& message) {
    if (error) {
        *error = message;
    }
}

// 模型常无视"不要代码块"的指令;剥掉包裹,只留首个 '['..最后 ']' 片段
QString extractJsonArray(const QString& content) {
    const int begin = content.indexOf(QLatin1Char('['));
    const int end = content.lastIndexOf(QLatin1Char(']'));
    if (begin < 0 || end <= begin) {
        return {};
    }
    return content.mid(begin, end - begin + 1);
}

QJsonArray parseArray(const QString& content, QString* error) {
    const QString payload = extractJsonArray(content);
    if (payload.isEmpty()) {
        setError(error, QCoreApplication::translate("textproto", "No JSON array in the response"));
        return {};
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        setError(error, QCoreApplication::translate("textproto", "Failed to parse the JSON array: %1")
                            .arg(parseError.errorString()));
        return {};
    }
    return doc.array();
}

} // namespace

QString openAiContent(const QByteArray& reply, QString* error) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QCoreApplication::translate("textproto", "The response is not valid JSON"));
        return {};
    }
    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("error"))) {
        const QJsonObject err = root.value(QLatin1String("error")).toObject();
        setError(error, QCoreApplication::translate("textproto", "Server error: %1")
                            .arg(err.value(QLatin1String("message"))
                                     .toString(QCoreApplication::translate("textproto", "(no details)"))));
        return {};
    }
    const QJsonArray choices = root.value(QLatin1String("choices")).toArray();
    if (choices.isEmpty()) {
        setError(error, QCoreApplication::translate("textproto", "The response has no choices"));
        return {};
    }
    const QString content = choices.first()
                                .toObject()
                                .value(QLatin1String("message"))
                                .toObject()
                                .value(QLatin1String("content"))
                                .toString();
    if (content.isEmpty()) {
        setError(error, QCoreApplication::translate("textproto", "The model returned an empty reply"));
    }
    return content;
}

QList<OcrLine> parseOcrLines(const QString& content, const QSize& imageSize,
                             QString* error) {
    const QJsonArray array = parseArray(content, error);
    struct RawLine {
        QString text;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        bool hasBox = false;
    };
    QList<RawLine> raw;
    double maxCoord = 0.0;
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        RawLine line;
        line.text = obj.value(QLatin1String("text")).toString().trimmed();
        if (line.text.isEmpty()) {
            continue;
        }
        const QJsonArray box = obj.value(QLatin1String("box")).toArray();
        if (box.size() == 4) {
            line.x1 = box.at(0).toDouble();
            line.y1 = box.at(1).toDouble();
            line.x2 = box.at(2).toDouble();
            line.y2 = box.at(3).toDouble();
            line.hasBox = true;
            maxCoord = std::max({maxCoord, line.x1, line.y1, line.x2, line.y2});
        }
        raw.append(line);
    }
    // 全部坐标 ≤1 → 模型按 0~1 归一化输出,按图像尺寸还原为像素
    const bool normalized = maxCoord > 0.0 && maxCoord <= 1.0 && imageSize.isValid();
    const double sx = normalized ? imageSize.width() : 1.0;
    const double sy = normalized ? imageSize.height() : 1.0;
    QList<OcrLine> lines;
    for (const RawLine& r : raw) {
        OcrLine line;
        line.text = r.text;
        if (r.hasBox) {
            line.box = QRect(QPoint(qRound(r.x1 * sx), qRound(r.y1 * sy)),
                             QPoint(qRound(r.x2 * sx), qRound(r.y2 * sy)))
                           .normalized();
        }
        lines.append(line);
    }
    return lines;
}

QStringList parseStringArray(const QString& content, QString* error) {
    const QJsonArray array = parseArray(content, error);
    QStringList result;
    for (const QJsonValue& value : array) {
        result.append(value.toString());
    }
    return result;
}

QList<OcrLine> parseUmiOcrReply(const QByteArray& reply, QString* error) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QCoreApplication::translate("textproto", "The Umi-OCR response is not valid JSON"));
        return {};
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QLatin1String("code")).toInt();
    if (code == 101) {
        return {}; // 图中无文字,不算错误
    }
    if (code != 100) {
        setError(error, QCoreApplication::translate("textproto", "Umi-OCR error (code %1): %2")
                            .arg(code)
                            .arg(root.value(QLatin1String("data")).toString()));
        return {};
    }
    QList<OcrLine> lines;
    const QJsonArray data = root.value(QLatin1String("data")).toArray();
    for (const QJsonValue& value : data) {
        const QJsonObject obj = value.toObject();
        OcrLine line;
        line.text = obj.value(QLatin1String("text")).toString().trimmed();
        if (line.text.isEmpty()) {
            continue;
        }
        // box 为四角点 [[x,y]×4](左上起),取包围矩形
        const QJsonArray corners = obj.value(QLatin1String("box")).toArray();
        if (corners.size() == 4) {
            int left = INT_MAX, top = INT_MAX, right = INT_MIN, bottom = INT_MIN;
            for (const QJsonValue& corner : corners) {
                const QJsonArray pt = corner.toArray();
                if (pt.size() != 2) {
                    continue;
                }
                const int x = qRound(pt.at(0).toDouble());
                const int y = qRound(pt.at(1).toDouble());
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
            if (right > left && bottom > top) {
                line.box = QRect(QPoint(left, top), QPoint(right, bottom));
            }
        }
        lines.append(line);
    }
    return lines;
}

QStringList parseDeepLReply(const QByteArray& reply, QString* error) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QCoreApplication::translate("textproto", "The DeepL response is not valid JSON"));
        return {};
    }
    const QJsonObject root = doc.object();
    if (!root.contains(QLatin1String("translations"))) {
        setError(error, QCoreApplication::translate("textproto", "DeepL error: %1")
                            .arg(root.value(QLatin1String("message"))
                                     .toString(QCoreApplication::translate("textproto", "(no details)"))));
        return {};
    }
    QStringList result;
    const QJsonArray translations = root.value(QLatin1String("translations")).toArray();
    for (const QJsonValue& value : translations) {
        result.append(value.toObject().value(QLatin1String("text")).toString());
    }
    return result;
}

QStringList parseDeepLXReply(const QByteArray& reply, QString* error) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QCoreApplication::translate(
                            "textproto", "The DeepLX response is not valid JSON"));
        return {};
    }
    const QJsonObject root = doc.object();
    const int code = root.value(QLatin1String("code")).toInt();
    if (code != 200) {
        setError(error,
                 QCoreApplication::translate("textproto", "DeepLX error (%1): %2")
                     .arg(code)
                     .arg(root.value(QLatin1String("message"))
                              .toString(QCoreApplication::translate(
                                  "textproto", "(no details)"))));
        return {};
    }
    // 多行原文以 \n 连接整体翻译,DeepL 保留换行 → 按 \n 拆回行
    return root.value(QLatin1String("data")).toString().split(QLatin1Char('\n'));
}

QStringList parseBaiduReply(const QByteArray& reply, QString* error) {
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QCoreApplication::translate("textproto", "The Baidu Translate response is not valid JSON"));
        return {};
    }
    const QJsonObject root = doc.object();
    // 百度成功响应无 error_code 字段;失败时 error_code 为字符串或数字
    if (root.contains(QLatin1String("error_code"))) {
        setError(error, QCoreApplication::translate("textproto", "Baidu Translate error (%1): %2")
                            .arg(root.value(QLatin1String("error_code"))
                                     .toVariant()
                                     .toString(),
                                 root.value(QLatin1String("error_msg")).toString()));
        return {};
    }
    QStringList result;
    const QJsonArray items = root.value(QLatin1String("trans_result")).toArray();
    for (const QJsonValue& value : items) {
        result.append(value.toObject().value(QLatin1String("dst")).toString());
    }
    return result;
}

} // namespace pixora::textproto
