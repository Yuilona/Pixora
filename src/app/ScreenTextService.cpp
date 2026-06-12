#include "app/ScreenTextService.h"

#include "app/PinService.h"
#include "app/SettingsService.h"
#include "core/text/ReplaceRenderer.h"
#include "ui/pin/PinWindow.h"

#include <QClipboard>
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
        emit failed(QStringLiteral("上一个识别/翻译任务还在进行中"));
        return;
    }
    const QString error = configError(false);
    if (!error.isEmpty()) {
        emit failed(error);
        return;
    }
    job_ = Job::Extract;
    image_ = image;
    emit started(QStringLiteral("正在识别文字…"));
    ocr_.recognize(image_, ocrConfig());
}

void ScreenTextService::translateInPlace(const QImage& image,
                                         const QRect& regionGlobal) {
    if (job_ != Job::None) {
        emit failed(QStringLiteral("上一个识别/翻译任务还在进行中"));
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
        pin_->setStatusBadge(QStringLiteral("翻译中…"));
    }
    ocr_.recognize(image_, ocrConfig());
}

void ScreenTextService::onOcrFinished(const QList<OcrLine>& lines) {
    if (job_ == Job::Extract) {
        job_ = Job::None;
        if (lines.isEmpty()) {
            emit failed(QStringLiteral("未识别到文字"));
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
            onFailed(QStringLiteral("未识别到文字"));
            return;
        }
        lines_ = lines;
        if (pin_) {
            pin_->setStatusBadge(QStringLiteral("翻译中…(%1 行)").arg(lines.size()));
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
            QStringLiteral("OCR 未返回文字位置,无法原位替换;译文已复制到剪贴板"));
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
    const auto ocr = ocrConfig();
    if (ocr.protocol == QLatin1String("openai") &&
        (ocr.baseUrl.isEmpty() || ocr.apiKey.isEmpty() || ocr.model.isEmpty())) {
        return QStringLiteral("请先在 设置 中填写 OCR 服务的地址、密钥与模型名");
    }
    if (!needTranslate) {
        return {};
    }
    const auto tr = translateConfig();
    if (tr.protocol == QLatin1String("openai") &&
        (tr.baseUrl.isEmpty() || tr.apiKey.isEmpty() || tr.model.isEmpty())) {
        return QStringLiteral("请先在 设置 中填写翻译服务的地址、密钥与模型名");
    }
    if (tr.protocol == QLatin1String("deepl") && tr.apiKey.isEmpty()) {
        return QStringLiteral("请先在 设置 中填写 DeepL 密钥");
    }
    if (tr.protocol == QLatin1String("baidu") &&
        (tr.appId.isEmpty() || tr.apiKey.isEmpty())) {
        return QStringLiteral("请先在 设置 中填写百度翻译的 APP ID 与密钥");
    }
    return {};
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
