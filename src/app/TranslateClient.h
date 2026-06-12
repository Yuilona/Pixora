#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>

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
    void translateOpenAi(const QStringList& lines, const Config& config);
    void translateDeepL(const QStringList& lines, const Config& config);
    void translateDeepLX(const QStringList& lines, const Config& config);
    void translateBaidu(const QStringList& lines, const Config& config);
    void deliver(QStringList translations, qsizetype expected);

    QNetworkAccessManager nam_;
};

} // namespace pixora
