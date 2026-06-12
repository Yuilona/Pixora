#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace pixora {

// 翻译协议客户端:OpenAI 兼容大模型 / DeepL / DeepLX(自托管)/
// 百度翻译开放平台。
// 一次 translate 恰好对应一次 finished 或 failed;译文与原文按下标对应,
// 行数不足时补空串(对应行原样保留),调用方无需关心各家差异。
class TranslateClient : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString protocol;   // "openai" | "deepl" | "deeplx" | "baidu"
        QString baseUrl;    // openai 必填;deepl/deeplx/baidu 留空用默认地址
        QString apiKey;     // openai key / DeepL auth key / DeepLX 访问令牌(可选)/ 百度密钥
        QString appId;      // 仅百度
        QString model;      // 仅 openai
        QString targetLang; // "zh" | "en" | "ja"
    };

    explicit TranslateClient(QObject* parent = nullptr);

    void translate(const QStringList& lines, const Config& config);

signals:
    void finished(const QStringList& translations);
    void failed(const QString& reason);

private:
    // 各家单请求都有规模上限(DeepL 50 条/128KiB,百度标准版 1000 字符,
    // LLM 输出 token 截断风险)——超限拆批串行发送,批内先对齐
    // (防跨批错位),全部完成才 finished;任一批失败整体 failed。
    struct BatchJob {
        QList<QStringList> batches;
        Config config;
        qsizetype expected = 0;
        QStringList results;
        int index = 0;
    };
    using BatchJobPtr = std::shared_ptr<BatchJob>;

    void translateOpenAi(const QStringList& lines, const Config& config);
    void translateDeepL(const QStringList& lines, const Config& config);
    void translateDeepLX(const QStringList& lines, const Config& config);
    void translateBaidu(const QStringList& lines, const Config& config);
    void sendOpenAiBatch(const BatchJobPtr& job);
    void sendDeepLBatch(const BatchJobPtr& job);
    void sendBaiduBatch(const BatchJobPtr& job);
    void accumulate(const BatchJobPtr& job, QStringList batchResult,
                    qsizetype batchSize, void (TranslateClient::*next)(const BatchJobPtr&),
                    int delayMs);
    void deliver(QStringList translations, qsizetype expected);

    QNetworkAccessManager nam_;
};

} // namespace pixora
