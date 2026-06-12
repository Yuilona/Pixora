#include "app/TranslateClient.h"

#include "core/text/TextProtocols.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrl>

#include <spdlog/spdlog.h>

namespace pixora {

namespace {

QString joinUrl(QString base, const QString& path) {
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (base.endsWith(path)) {
        return base;
    }
    return base + path;
}

QString httpFailureReason(QNetworkReply* reply) {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString excerpt = QString::fromUtf8(reply->readAll().left(300));
    return QStringLiteral("HTTP %1 %2:%3")
        .arg(status)
        .arg(reply->errorString(), excerpt);
}

// 各家目标语言代码不一致:DeepL 用大写(英语须带地区),百度日语是 jp
QString langFor(const QString& protocol, const QString& target) {
    if (protocol == QLatin1String("deepl")) {
        if (target == QLatin1String("en")) return QStringLiteral("EN-US");
        if (target == QLatin1String("ja")) return QStringLiteral("JA");
        return QStringLiteral("ZH");
    }
    if (protocol == QLatin1String("baidu")) {
        if (target == QLatin1String("en")) return QStringLiteral("en");
        if (target == QLatin1String("ja")) return QStringLiteral("jp");
        return QStringLiteral("zh");
    }
    // LLM 提示词用自然语言
    if (target == QLatin1String("en")) return QStringLiteral("英文");
    if (target == QLatin1String("ja")) return QStringLiteral("日文");
    return QStringLiteral("中文");
}

} // namespace

TranslateClient::TranslateClient(QObject* parent) : QObject(parent) {
    nam_.setTransferTimeout(60'000);
}

void TranslateClient::translate(const QStringList& lines, const Config& config) {
    if (lines.isEmpty()) {
        emit finished({});
        return;
    }
    if (config.protocol == QLatin1String("deepl")) {
        translateDeepL(lines, config);
    } else if (config.protocol == QLatin1String("baidu")) {
        translateBaidu(lines, config);
    } else {
        translateOpenAi(lines, config);
    }
}

// 行数对齐后发出:多了截断,少了补空串(对应行原样保留)
void TranslateClient::deliver(QStringList translations, qsizetype expected) {
    if (translations.size() != expected) {
        spdlog::warn("translate: got {} line(s), expected {}", translations.size(),
                     expected);
        while (translations.size() < expected) {
            translations.append(QString());
        }
        translations = translations.mid(0, expected);
    }
    emit finished(translations);
}

void TranslateClient::translateOpenAi(const QStringList& lines, const Config& config) {
    QJsonArray input;
    for (const QString& line : lines) {
        input.append(line);
    }
    const QString prompt =
        QStringLiteral(
            "你是翻译引擎。把下面 JSON 数组中的每个字符串翻译成%1。"
            "严格输出 JSON 字符串数组,长度与输入相同、顺序一一对应;"
            "不要输出任何解释或代码块标记。已是%1的条目原样返回。\n输入:\n%2")
            .arg(langFor(config.protocol, config.targetLang),
                 QString::fromUtf8(
                     QJsonDocument(input).toJson(QJsonDocument::Compact)));

    const QJsonObject body{
        {QLatin1String("model"), config.model},
        {QLatin1String("temperature"), 0},
        {QLatin1String("messages"),
         QJsonArray{QJsonObject{{QLatin1String("role"), QLatin1String("user")},
                                {QLatin1String("content"), prompt}}}}};

    QNetworkRequest request(QUrl(joinUrl(config.baseUrl,
                                         QStringLiteral("/chat/completions"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());

    spdlog::info("translate(openai): POST {} model={} lines={}",
                 request.url().toString().toStdString(), config.model.toStdString(),
                 lines.size());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, expected = lines.size()] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    QString serverMessage;
                    textproto::openAiContent(reply->peek(reply->bytesAvailable()),
                                             &serverMessage);
                    emit failed(serverMessage.isEmpty() ? httpFailureReason(reply)
                                                        : serverMessage);
                    return;
                }
                QString error;
                const QString content =
                    textproto::openAiContent(reply->readAll(), &error);
                if (content.isEmpty()) {
                    emit failed(error);
                    return;
                }
                const QStringList translations =
                    textproto::parseStringArray(content, &error);
                if (translations.isEmpty() && !error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                deliver(translations, expected);
            });
}

void TranslateClient::translateDeepL(const QStringList& lines, const Config& config) {
    const QString base = config.baseUrl.isEmpty()
                             ? QStringLiteral("https://api-free.deepl.com")
                             : config.baseUrl;
    QJsonArray text;
    for (const QString& line : lines) {
        text.append(line);
    }
    const QJsonObject body{
        {QLatin1String("text"), text},
        {QLatin1String("target_lang"), langFor(config.protocol, config.targetLang)}};

    QNetworkRequest request(QUrl(joinUrl(base, QStringLiteral("/v2/translate"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         "DeepL-Auth-Key " + config.apiKey.toUtf8());

    spdlog::info("translate(deepl): POST {} lines={}",
                 request.url().toString().toStdString(), lines.size());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, expected = lines.size()] {
                reply->deleteLater();
                QString error;
                const QStringList translations =
                    textproto::parseDeepLReply(reply->readAll(), &error);
                if (!error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                if (reply->error() != QNetworkReply::NoError &&
                    translations.isEmpty()) {
                    emit failed(httpFailureReason(reply));
                    return;
                }
                deliver(translations, expected);
            });
}

void TranslateClient::translateBaidu(const QStringList& lines, const Config& config) {
    const QString url = config.baseUrl.isEmpty()
                            ? QStringLiteral(
                                  "https://fanyi-api.baidu.com/api/trans/vip/translate")
                            : config.baseUrl;
    // 多行以 \n 连接,百度按行返回 trans_result 数组
    const QString q = lines.join(QLatin1Char('\n'));
    const QString salt =
        QString::number(QRandomGenerator::global()->generate());
    // sign = MD5(appid + q + salt + 密钥),q 此处不做 URL 编码(官方坑点)
    const QByteArray sign =
        QCryptographicHash::hash((config.appId + q + salt + config.apiKey).toUtf8(),
                                 QCryptographicHash::Md5)
            .toHex();

    QByteArray form;
    form += "q=" + QUrl::toPercentEncoding(q);
    form += "&from=auto&to=" +
            langFor(config.protocol, config.targetLang).toUtf8();
    form += "&appid=" + QUrl::toPercentEncoding(config.appId);
    form += "&salt=" + salt.toUtf8();
    form += "&sign=" + sign;

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    spdlog::info("translate(baidu): POST {} lines={}", url.toStdString(),
                 lines.size());
    QNetworkReply* reply = nam_.post(request, form);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, expected = lines.size()] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    emit failed(httpFailureReason(reply));
                    return;
                }
                QString error;
                const QStringList translations =
                    textproto::parseBaiduReply(reply->readAll(), &error);
                if (!error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                deliver(translations, expected);
            });
}

} // namespace pixora
