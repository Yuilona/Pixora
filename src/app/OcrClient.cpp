#include "app/OcrClient.h"

#include "app/ApiHelpers.h"
#include "core/text/TextProtocols.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace pixora {

namespace {

// 视觉模型上传上限:过大图片徒增 token 与延迟,识别率反而不稳
constexpr int kMaxUploadSide = 2048;

QByteArray toPngBase64(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes.toBase64();
}

} // namespace

OcrClient::OcrClient(QObject* parent) : QObject(parent) {
    nam_.setTransferTimeout(60'000);
}

void OcrClient::recognize(const QImage& image, const Config& config) {
    if (image.isNull()) {
        emit failed(tr("No image to recognize"));
        return;
    }
    if (config.protocol == QLatin1String("umiocr")) {
        recognizeUmiOcr(image, config);
    } else {
        recognizeOpenAi(image, config);
    }
}

void OcrClient::recognizeOpenAi(const QImage& image, const Config& config) {
    // 超大区域先降采样;识别框按比例放回原图坐标
    QImage upload = image;
    qreal scaleBack = 1.0;
    const int side = std::max(upload.width(), upload.height());
    if (side > kMaxUploadSide) {
        scaleBack = qreal(side) / kMaxUploadSide;
        upload = upload.scaled(qRound(upload.width() / scaleBack),
                               qRound(upload.height() / scaleBack),
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 给模型的指令,非 UI 文案——统一英文,主流中外模型均遵循良好
    const QString prompt =
        QStringLiteral(
            "Recognize every line of text in the image. Output strictly a JSON "
            "array and nothing else (no code fences): "
            "[{\"text\":\"line text\",\"box\":[x1,y1,x2,y2]}]. "
            "box is the pixel rectangle of the line in the image (top-left "
            "x1,y1; bottom-right x2,y2); the image is %1 px wide and %2 px "
            "tall. Order lines by reading order; use [0,0,0,0] when the "
            "position is unknown; output [] if the image has no text.")
            .arg(upload.width())
            .arg(upload.height());

    QJsonObject imagePart;
    imagePart.insert(QLatin1String("type"), QLatin1String("image_url"));
    imagePart.insert(
        QLatin1String("image_url"),
        QJsonObject{{QLatin1String("url"),
                     QStringLiteral("data:image/png;base64,") +
                         QString::fromLatin1(toPngBase64(upload))}});
    const QJsonObject body{
        {QLatin1String("model"), config.model},
        {QLatin1String("temperature"), 0},
        {QLatin1String("messages"),
         QJsonArray{QJsonObject{
             {QLatin1String("role"), QLatin1String("user")},
             {QLatin1String("content"),
              QJsonArray{
                  QJsonObject{{QLatin1String("type"), QLatin1String("text")},
                              {QLatin1String("text"), prompt}},
                  imagePart}}}}}};

    QNetworkRequest request(QUrl(net::joinUrl(config.baseUrl,
                                         QStringLiteral("/chat/completions"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + config.apiKey.toUtf8());

    spdlog::info("ocr(openai): POST {} model={} image={}x{}",
                 request.url().toString().toStdString(), config.model.toStdString(),
                 upload.width(), upload.height());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, scaleBack, size = upload.size()] {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    // 错误体里常有比 errorString 更有用的服务端消息
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
                QList<OcrLine> lines =
                    textproto::parseOcrLines(content, size, &error);
                if (lines.isEmpty() && !error.isEmpty()) {
                    emit failed(error);
                    return;
                }
                if (scaleBack != 1.0) {
                    for (OcrLine& line : lines) {
                        line.box = QRect(qRound(line.box.x() * scaleBack),
                                         qRound(line.box.y() * scaleBack),
                                         qRound(line.box.width() * scaleBack),
                                         qRound(line.box.height() * scaleBack));
                    }
                }
                spdlog::info("ocr(openai): {} line(s)", lines.size());
                emit finished(lines);
            });
}

void OcrClient::recognizeUmiOcr(const QImage& image, const Config& config) {
    const QString base = config.baseUrl.isEmpty()
                             ? QStringLiteral("http://127.0.0.1:1224")
                             : config.baseUrl;
    const QJsonObject body{
        {QLatin1String("base64"), QString::fromLatin1(toPngBase64(image))},
        {QLatin1String("options"),
         QJsonObject{{QLatin1String("data.format"), QLatin1String("dict")}}}};

    QNetworkRequest request(QUrl(net::joinUrl(base, QStringLiteral("/api/ocr"))));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    spdlog::info("ocr(umiocr): POST {} image={}x{}",
                 request.url().toString().toStdString(), image.width(),
                 image.height());
    QNetworkReply* reply =
        nam_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(net::httpFailureReason(reply));
            return;
        }
        QString error;
        const QList<OcrLine> lines =
            textproto::parseUmiOcrReply(reply->readAll(), &error);
        if (!error.isEmpty()) {
            emit failed(error);
            return;
        }
        spdlog::info("ocr(umiocr): {} line(s)", lines.size());
        emit finished(lines);
    });
}

} // namespace pixora
