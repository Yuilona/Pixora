#include "app/TranslateClient.h"

#include "app/ApiHelpers.h"
#include "core/text/TextProtocols.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrl>

#include <spdlog/spdlog.h>

namespace pixora {

namespace {

// 各家目标语言代码不一致:DeepL 用大写(英语须带地区),百度日语是 jp
QString langFor(const QString& protocol, const QString& target) {
    // DeepLX 最终把 target_lang 透传给 DeepL,语言代码体系一致
    if (protocol == QLatin1String("deepl") || protocol == QLatin1String("deeplx")) {
        if (target == QLatin1String("en")) return QStringLiteral("EN-US");
        if (target == QLatin1String("ja")) return QStringLiteral("JA");
        return QStringLiteral("ZH");
    }
    if (protocol == QLatin1String("baidu")) {
        if (target == QLatin1String("en")) return QStringLiteral("en");
        if (target == QLatin1String("ja")) return QStringLiteral("jp");
        return QStringLiteral("zh");
    }
    // LLM 提示词用自然语言(提示词统一英文,见 translateOpenAi)
    if (target == QLatin1String("en")) return QStringLiteral("English");
    if (target == QLatin1String("ja")) return QStringLiteral("Japanese");
    return QStringLiteral("Simplified Chinese");
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
    } else if (config.protocol == QLatin1String("deeplx")) {
        translateDeepLX(lines, config);
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

// 批完成的公共账务:批内补齐/截断(防跨批错位)→ 续发下一批或交付
void TranslateClient::accumulate(const BatchJobPtr& job, QStringList batchResult,
                                 qsizetype batchSize,
                                 void (TranslateClient::*next)(const BatchJobPtr&),
                                 int delayMs) {
    if (batchResult.size() != batchSize) {
        spdlog::warn("translate: batch {} got {} line(s), expected {}", job->index,
                     batchResult.size(), batchSize);
        while (batchResult.size() < batchSize) {
            batchResult.append(QString());
        }
        batchResult = batchResult.mid(0, batchSize);
    }
    job->results += batchResult;
    ++job->index;
    if (job->index >= job->batches.size()) {
        deliver(job->results, job->expected);
        return;
    }
    if (delayMs > 0) {
        QTimer::singleShot(delayMs, this, [this, job, next] { (this->*next)(job); });
    } else {
        (this->*next)(job);
    }
}

void TranslateClient::translateOpenAi(const QStringList& lines, const Config& config) {
    // 输出会被厂商默认 max_tokens 截断 → 部分行静默不译;
    // 控制单批输入规模 + 显式 max_tokens 双保险
    auto job = std::make_shared<BatchJob>();
    job->batches = textproto::splitLineBatches(lines, 40, -1, 4000);
    job->config = config;
    job->expected = lines.size();
    sendOpenAiBatch(job);
}

void TranslateClient::sendOpenAiBatch(const BatchJobPtr& job) {
    const QStringList& batch = job->batches[job->index];
    const Config& config = job->config;
    QJsonArray input;
    for (const QString& line : batch) {
        input.append(line);
    }
    // 给模型的指令,非 UI 文案——统一英文
    const QString prompt =
        QStringLiteral(
            "You are a translation engine. Translate every string in the JSON "
            "array below into %1. Output strictly a JSON array of strings with "
            "the same length and order as the input; no explanations, no code "
            "fences. Return entries already in %1 unchanged.\nInput:\n%2")
            .arg(langFor(config.protocol, config.targetLang),
                 QString::fromUtf8(
                     QJsonDocument(input).toJson(QJsonDocument::Compact)));

    const QJsonObject body{
        {QLatin1String("model"), config.model},
        {QLatin1String("temperature"), 0},
        // 4096 当前各家普遍支持;过大反而会在低上限模型上 400
        {QLatin1String("max_tokens"), 4096},
        {QLatin1String("messages"),
         QJsonArray{QJsonObject{{QLatin1String("role"), QLatin1String("user")},
                                {QLatin1String("content"), prompt}}}}};

    QNetworkRequest request(QUrl(net::joinUrl(config.baseUrl,
                                         QStringLiteral("/chat/completions"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());

    spdlog::info("translate(openai): POST {} model={} batch {}/{} lines={}",
                 request.url().toString().toStdString(), config.model.toStdString(),
                 job->index + 1, job->batches.size(), batch.size());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, job, batchSize = batch.size()] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    QString serverMessage;
                    textproto::openAiContent(reply->peek(reply->bytesAvailable()),
                                             &serverMessage);
                    emit failed(serverMessage.isEmpty() ? net::httpFailureReason(reply)
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
                accumulate(job, translations, batchSize,
                           &TranslateClient::sendOpenAiBatch, 0);
            });
}

void TranslateClient::translateDeepL(const QStringList& lines, const Config& config) {
    // 官方限单请求 50 条 / 128KiB 请求体,留余量分批
    auto job = std::make_shared<BatchJob>();
    job->batches = textproto::splitLineBatches(lines, 50, 110 * 1024, -1);
    job->config = config;
    job->expected = lines.size();
    sendDeepLBatch(job);
}

void TranslateClient::sendDeepLBatch(const BatchJobPtr& job) {
    const QStringList& batch = job->batches[job->index];
    const Config& config = job->config;
    QString base = config.baseUrl;
    if (base.isEmpty()) {
        // 免费版 key 以 ":fx" 结尾,Pro key 走 api.deepl.com
        base = config.apiKey.endsWith(QLatin1String(":fx"))
                   ? QStringLiteral("https://api-free.deepl.com")
                   : QStringLiteral("https://api.deepl.com");
    }
    QJsonArray text;
    for (const QString& line : batch) {
        text.append(line);
    }
    const QJsonObject body{
        {QLatin1String("text"), text},
        {QLatin1String("target_lang"), langFor(config.protocol, config.targetLang)}};

    QNetworkRequest request(QUrl(net::joinUrl(base, QStringLiteral("/v2/translate"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         "DeepL-Auth-Key " + config.apiKey.toUtf8());

    spdlog::info("translate(deepl): POST {} batch {}/{} lines={}",
                 request.url().toString().toStdString(), job->index + 1,
                 job->batches.size(), batch.size());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, job, batchSize = batch.size()] {
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
                    emit failed(net::httpFailureReason(reply));
                    return;
                }
                accumulate(job, translations, batchSize,
                           &TranslateClient::sendDeepLBatch, 0);
            });
}

// DeepLX 自托管服务的免费端点 /translate:text 是单字符串而非数组
// (其 /v2/translate 虽收数组,但 join 后整体翻译、只回一个元素,
// 行对应会丢)——多行以 \n 连接,译文按 \n 拆回,行数漂移由
// deliver() 对齐兜底。访问令牌可选,Bearer 头携带。
void TranslateClient::translateDeepLX(const QStringList& lines, const Config& config) {
    const QString base = config.baseUrl.isEmpty()
                             ? QStringLiteral("http://127.0.0.1:1188")
                             : config.baseUrl;
    const QJsonObject body{
        {QLatin1String("text"), lines.join(QLatin1Char('\n'))},
        {QLatin1String("source_lang"), QLatin1String("auto")},
        {QLatin1String("target_lang"), langFor(config.protocol, config.targetLang)}};

    QNetworkRequest request(QUrl(net::joinUrl(base, QStringLiteral("/translate"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!config.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());
    }

    spdlog::info("translate(deeplx): POST {} lines={}",
                 request.url().toString().toStdString(), lines.size());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, expected = lines.size()] {
                reply->deleteLater();
                QString error;
                const QStringList translations =
                    textproto::parseDeepLXReply(reply->readAll(), &error);
                if (!error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                if (reply->error() != QNetworkReply::NoError &&
                    translations.isEmpty()) {
                    emit failed(net::httpFailureReason(reply));
                    return;
                }
                deliver(translations, expected);
            });
}

void TranslateClient::translateBaidu(const QStringList& lines, const Config& config) {
    // 标准版限单请求 1000 字符(高级版 6000),按 950 分批保守适配;
    // 标准版 QPS=1,批间 1.1s 节流(见 sendBaiduBatch)
    auto job = std::make_shared<BatchJob>();
    job->batches = textproto::splitLineBatches(lines, -1, -1, 950);
    job->config = config;
    job->expected = lines.size();
    sendBaiduBatch(job);
}

void TranslateClient::sendBaiduBatch(const BatchJobPtr& job) {
    const QStringList& batch = job->batches[job->index];
    const Config& config = job->config;
    const QString url = config.baseUrl.isEmpty()
                            ? QStringLiteral(
                                  "https://fanyi-api.baidu.com/api/trans/vip/translate")
                            : config.baseUrl;
    // 多行以 \n 连接,百度按行返回 trans_result 数组
    const QString q = batch.join(QLatin1Char('\n'));
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

    spdlog::info("translate(baidu): POST {} batch {}/{} lines={}", url.toStdString(),
                 job->index + 1, job->batches.size(), batch.size());
    QNetworkReply* reply = nam_.post(request, form);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, job, batchSize = batch.size()] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    emit failed(net::httpFailureReason(reply));
                    return;
                }
                QString error;
                const QStringList translations =
                    textproto::parseBaiduReply(reply->readAll(), &error);
                if (!error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                accumulate(job, translations, batchSize,
                           &TranslateClient::sendBaiduBatch, 1100);
            });
}

} // namespace pixora
