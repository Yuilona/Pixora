#pragma once

#include "core/text/OcrTypes.h"

#include <QImage>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

namespace pixora {

// OCR 协议客户端:OpenAI 兼容视觉模型 / Umi-OCR(PaddleOCR 系)本地服务。
// 一次 recognize 恰好对应一次 finished 或 failed;调用方负责串行化。
class OcrClient : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString protocol; // "openai" | "umiocr"
        QString baseUrl;
        QString apiKey; // umiocr 不需要
        QString model;  // 仅 openai
    };

    explicit OcrClient(QObject* parent = nullptr);

    void recognize(const QImage& image, const Config& config);

signals:
    void finished(const QList<OcrLine>& lines);
    void failed(const QString& reason);

private:
    void recognizeOpenAi(const QImage& image, const Config& config);
    void recognizeUmiOcr(const QImage& image, const Config& config);

    QNetworkAccessManager nam_;
};

} // namespace pixora
