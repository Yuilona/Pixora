#include "app/ScreenTextService.h"

#include "app/PinService.h"
#include "app/SettingsService.h"
#include "core/text/ReplaceRenderer.h"
#include "ui/pin/PinWindow.h"

#include <QClipboard>
#include <QPainter>
#include <QGuiApplication>

#include <spdlog/spdlog.h>

namespace pixora {

ScreenTextService::ScreenTextService(const SettingsService* settings,
                                     PinService* pins, QObject* parent)
    : QObject(parent), settings_(settings), pins_(pins) {
    connect(&ocr_, &OcrClient::finished, this, &ScreenTextService::onOcrFinished);
    connect(&ocr_, &OcrClient::failed, this, &ScreenTextService::onFailed);
    connect(&translator_, &TranslateClient::finished, this,
            &ScreenTextService::onTranslated);
    connect(&translator_, &TranslateClient::failed, this,
            &ScreenTextService::onFailed);
}

void ScreenTextService::extractText(const QImage& image) {
    if (job_ != Job::None) {
        emit failed(tr("The previous OCR/translate job is still running"));
        return;
    }
    const QString error = configError(false);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }
    job_ = Job::Extract;
    image_ = image;
    emit started(tr("Recognizing text..."));
    ocr_.recognize(image_, ocrConfig());
}

void ScreenTextService::translateInPlace(const QImage& image,
                                         const QRect& regionGlobal) {
    if (job_ != Job::None) {
        emit failed(tr("The previous OCR/translate job is still running"));
        return;
    }
    const QString error = configError(true);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }
    job_ = Job::Translate;
    image_ = image;
    // 原图先原位贴出,识别/翻译期间画面不缺口;完成后原地换图
    pin_ = pins_ ? pins_->pinImage(image, regionGlobal.topLeft()) : nullptr;
    if (pin_) {
        pin_->setStatusBadge(tr("Translating..."));
    }
    ocr_.recognize(image_, ocrConfig());
}

void ScreenTextService::onOcrFinished(const QList<OcrLine>& lines) {
    if (job_ == Job::Extract) {
        job_ = Job::None;
        if (lines.isEmpty()) {
            emit failed(tr("No text recognized"));
            return;
        }
        QStringList texts;
        for (const OcrLine& line : lines) {
            texts.append(line.text);
        }
        QGuiApplication::clipboard()->setText(texts.join(QLatin1Char('\n')));
        emit textCopied(int(lines.size()));
        return;
    }
    if (job_ == Job::Translate) {
        if (lines.isEmpty()) {
            onFailed(tr("No text recognized"));
            return;
        }
        lines_ = lines;
        if (pin_) {
            pin_->setStatusBadge(tr("Translating... (%1 lines)").arg(lines.size()));
        }
        QStringList texts;
        for (const OcrLine& line : lines_) {
            texts.append(line.text);
        }
        translator_.translate(texts, translateConfig());
    }
}

void ScreenTextService::onTranslated(const QStringList& translations) {
    if (job_ != Job::Translate) {
        return;
    }
    job_ = Job::None;

    int validBoxes = 0;
    for (const OcrLine& line : lines_) {
        if (line.box.width() > 0 && line.box.height() > 0) {
            ++validBoxes;
        }
    }
    if (validBoxes == 0) {
        // 模型没给位置 → 无法回绘;译文进剪贴板,贴图保持原样
        QGuiApplication::clipboard()->setText(
            translations.join(QLatin1Char('\n')));
        if (pin_) {
            pin_->setStatusBadge(QString());
        }
        emit failed(
            tr("OCR returned no text positions, so in-place replacement is not "
               "possible; the translation was copied to the clipboard"));
        return;
    }

    const QImage replaced = renderReplacedText(image_, lines_, translations);
    if (pin_) {
        pin_->setImage(replaced);
        pin_->setStatusBadge(QString());
    }
    spdlog::info("translate in place: {} line(s), {} with box", lines_.size(),
                 validBoxes);
    emit translateFinished(int(lines_.size()));
}

void ScreenTextService::onFailed(const QString& reason) {
    job_ = Job::None;
    if (pin_) {
        pin_->setStatusBadge(QString()); // 贴图保留原图,可手动关闭
    }
    spdlog::warn("screen text job failed: {}", reason.toStdString());
    emit failed(reason);
}

QString ScreenTextService::configError(bool needTranslate) const {
    if (const QString error = ocrConfigError(ocrConfig()); !error.isEmpty()) {
        return error;
    }
    return needTranslate ? translateConfigError(translateConfig()) : QString();
}

QString ScreenTextService::ocrConfigError(const OcrClient::Config& ocr) {
    if (ocr.protocol == QLatin1String("openai") &&
        (ocr.baseUrl.isEmpty() || ocr.apiKey.isEmpty() || ocr.model.isEmpty())) {
        return tr("Fill in the OCR service endpoint, API key and model in "
                  "Settings first");
    }
    return {};
}

QString ScreenTextService::translateConfigError(const TranslateClient::Config& trc) {
    if (trc.protocol == QLatin1String("openai") &&
        (trc.baseUrl.isEmpty() || trc.apiKey.isEmpty() || trc.model.isEmpty())) {
        return tr("Fill in the translation service endpoint, API key and model "
                  "in Settings first");
    }
    if (trc.protocol == QLatin1String("deepl") && trc.apiKey.isEmpty()) {
        return tr("Fill in the DeepL API key in Settings first");
    }
    if (trc.protocol == QLatin1String("baidu") &&
        (trc.appId.isEmpty() || trc.apiKey.isEmpty())) {
        return tr("Fill in the Baidu Translate APP ID and secret key in "
                  "Settings first");
    }
    return {};
}

void ScreenTextService::testOcr(const OcrClient::Config& config) {
    if (const QString error = ocrConfigError(config); !error.isEmpty()) {
        emit ocrTestFinished(false, error);
        return;
    }
    // 样张:白底黑字一行,任何 OCR 都应能认出
    QImage sample(420, 96, QImage::Format_RGB32);
    sample.fill(Qt::white);
    {
        QPainter p(&sample);
        QFont font;
        font.setPixelSize(44);
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(sample.rect(), Qt::AlignCenter, QStringLiteral("Pixora 2026"));
    }
    auto* client = new OcrClient(this);
    connect(client, &OcrClient::finished, this, [this, client](const QList<OcrLine>& lines) {
        client->deleteLater();
        QStringList texts;
        for (const OcrLine& line : lines) {
            texts << line.text;
        }
        const QString joined = texts.join(QLatin1Char(' ')).simplified();
        if (joined.isEmpty()) {
            emit ocrTestFinished(false, tr("Connected, but no text was recognized"));
        } else {
            emit ocrTestFinished(true, tr("Recognized: %1").arg(joined));
        }
    });
    connect(client, &OcrClient::failed, this, [this, client](const QString& reason) {
        client->deleteLater();
        emit ocrTestFinished(false, reason);
    });
    client->recognize(sample, config);
}

void ScreenTextService::testTranslate(const TranslateClient::Config& config) {
    if (const QString error = translateConfigError(config); !error.isEmpty()) {
        emit translateTestFinished(false, error);
        return;
    }
    auto* client = new TranslateClient(this);
    connect(client, &TranslateClient::finished, this,
            [this, client](const QStringList& translations) {
                client->deleteLater();
                const QString text = translations.value(0).trimmed();
                if (text.isEmpty()) {
                    emit translateTestFinished(false,
                                               tr("Connected, but the reply was empty"));
                } else {
                    emit translateTestFinished(true, tr("Translated: %1").arg(text));
                }
            });
    connect(client, &TranslateClient::failed, this, [this, client](const QString& reason) {
        client->deleteLater();
        emit translateTestFinished(false, reason);
    });
    client->translate({QStringLiteral("Hello, world!")}, config);
}

OcrClient::Config ScreenTextService::ocrConfig() const {
    OcrClient::Config config;
    config.protocol = settings_->ocrProtocol();
    config.baseUrl = settings_->ocrBaseUrl();
    config.apiKey = settings_->ocrApiKey();
    config.model = settings_->ocrModel();
    return config;
}

TranslateClient::Config ScreenTextService::translateConfig() const {
    TranslateClient::Config config;
    config.protocol = settings_->translateProtocol();
    config.baseUrl = settings_->translateBaseUrl();
    config.apiKey = settings_->translateApiKey();
    config.appId = settings_->translateAppId();
    config.model = settings_->translateModel();
    config.targetLang = settings_->translateTargetLang();
    return config;
}

} // namespace pixora
