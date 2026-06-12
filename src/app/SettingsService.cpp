#include "app/SettingsService.h"

#include <QJsonValue>

#include <algorithm>

namespace pixora {

namespace {
namespace keys {
// 不用 "general/" 前缀:INI 格式里 General 是保留段名,会被写成 [%General]
const QString language = QStringLiteral("ui/language");
const QString hotkeyCaptureRegion = QStringLiteral("hotkeys/captureRegion");
const QString hotkeyPinFromClipboard = QStringLiteral("hotkeys/pinFromClipboard");
const QString outputDir = QStringLiteral("output/dir");
const QString historyLimit = QStringLiteral("history/limit");
const QString fileNameTemplate = QStringLiteral("output/fileTemplate");
const QString outputFormat = QStringLiteral("output/format");
const QString outputQuality = QStringLiteral("output/quality");
const QString autoSave = QStringLiteral("output/autoSave");
const QString ocrProtocol = QStringLiteral("ai/ocrProtocol");
const QString ocrBaseUrl = QStringLiteral("ai/ocrBaseUrl");
const QString ocrApiKey = QStringLiteral("ai/ocrApiKey");
const QString ocrModel = QStringLiteral("ai/ocrModel");
const QString trProtocol = QStringLiteral("ai/translateProtocol");
const QString trBaseUrl = QStringLiteral("ai/translateBaseUrl");
const QString trApiKey = QStringLiteral("ai/translateApiKey");
const QString trAppId = QStringLiteral("ai/translateAppId");
const QString trModel = QStringLiteral("ai/translateModel");
const QString trTargetLang = QStringLiteral("ai/translateTargetLang");
} // namespace keys

// 默认热键取通行键位习惯:F1 截图、F3 贴图;长截图走截图工具栏,无独立热键。
const QString defaultCaptureRegion = QStringLiteral("F1");
const QString defaultPinFromClipboard = QStringLiteral("F3");

QKeySequence toKeySequence(const QVariant& v) {
    return QKeySequence(v.toString(), QKeySequence::PortableText);
}
} // namespace

SettingsService::SettingsService(const QString& iniPath, QObject* parent)
    : QObject(parent),
      settings_(iniPath.isEmpty()
                    ? QSettings(QSettings::IniFormat, QSettings::UserScope,
                                QStringLiteral("Pixora"), QStringLiteral("Pixora"))
                          .fileName()
                    : iniPath,
                QSettings::IniFormat) {}

QString SettingsService::language() const {
    const QString lang = value(keys::language, QStringLiteral("auto")).toString();
    if (lang == QLatin1String("zh_CN") || lang == QLatin1String("en")) {
        return lang;
    }
    return QStringLiteral("auto");
}

void SettingsService::setLanguage(const QString& lang) {
    setValue(keys::language, lang);
}

QKeySequence SettingsService::hotkeyCaptureRegion() const {
    return toKeySequence(value(keys::hotkeyCaptureRegion, defaultCaptureRegion));
}

QKeySequence SettingsService::hotkeyPinFromClipboard() const {
    return toKeySequence(value(keys::hotkeyPinFromClipboard, defaultPinFromClipboard));
}

void SettingsService::setHotkeyCaptureRegion(const QKeySequence& seq) {
    setValue(keys::hotkeyCaptureRegion, seq.toString(QKeySequence::PortableText));
}

void SettingsService::setHotkeyPinFromClipboard(const QKeySequence& seq) {
    setValue(keys::hotkeyPinFromClipboard, seq.toString(QKeySequence::PortableText));
}

QString SettingsService::outputDir() const {
    return value(keys::outputDir, QString()).toString();
}

void SettingsService::setOutputDir(const QString& dir) {
    setValue(keys::outputDir, dir);
}

int SettingsService::historyLimit() const {
    return value(keys::historyLimit, 20).toInt();
}

void SettingsService::setHistoryLimit(int count) {
    setValue(keys::historyLimit, count);
}

QString SettingsService::fileNameTemplate() const {
    return value(keys::fileNameTemplate,
                 QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"))
        .toString();
}

void SettingsService::setFileNameTemplate(const QString& tmpl) {
    setValue(keys::fileNameTemplate, tmpl);
}

QString SettingsService::outputFormat() const {
    // 仅 png/jpg:本机 Qt 未装 qtimageformats 模块,WebP 编码不可用
    const QString fmt = value(keys::outputFormat, QStringLiteral("png")).toString();
    return fmt == QLatin1String("jpg") ? fmt : QStringLiteral("png");
}

void SettingsService::setOutputFormat(const QString& format) {
    setValue(keys::outputFormat, format);
}

int SettingsService::outputQuality() const {
    return std::clamp(value(keys::outputQuality, 90).toInt(), 1, 100);
}

void SettingsService::setOutputQuality(int quality) {
    setValue(keys::outputQuality, quality);
}

bool SettingsService::autoSave() const {
    return value(keys::autoSave, false).toBool();
}

void SettingsService::setAutoSave(bool enabled) {
    setValue(keys::autoSave, enabled);
}

QString SettingsService::ocrProtocol() const {
    const QString p = value(keys::ocrProtocol, QStringLiteral("openai")).toString();
    return p == QLatin1String("umiocr") ? p : QStringLiteral("openai");
}

void SettingsService::setOcrProtocol(const QString& protocol) {
    setValue(keys::ocrProtocol, protocol);
}

QString SettingsService::ocrBaseUrl() const {
    return value(keys::ocrBaseUrl, QString()).toString().trimmed();
}

void SettingsService::setOcrBaseUrl(const QString& url) {
    setValue(keys::ocrBaseUrl, url.trimmed());
}

QString SettingsService::ocrApiKey() const {
    return value(keys::ocrApiKey, QString()).toString().trimmed();
}

void SettingsService::setOcrApiKey(const QString& key) {
    setValue(keys::ocrApiKey, key.trimmed());
}

QString SettingsService::ocrModel() const {
    return value(keys::ocrModel, QString()).toString().trimmed();
}

void SettingsService::setOcrModel(const QString& model) {
    setValue(keys::ocrModel, model.trimmed());
}

QString SettingsService::translateProtocol() const {
    const QString p = value(keys::trProtocol, QStringLiteral("openai")).toString();
    if (p == QLatin1String("deepl") || p == QLatin1String("deeplx") ||
        p == QLatin1String("baidu")) {
        return p;
    }
    return QStringLiteral("openai");
}

void SettingsService::setTranslateProtocol(const QString& protocol) {
    setValue(keys::trProtocol, protocol);
}

QString SettingsService::translateBaseUrl() const {
    return value(keys::trBaseUrl, QString()).toString().trimmed();
}

void SettingsService::setTranslateBaseUrl(const QString& url) {
    setValue(keys::trBaseUrl, url.trimmed());
}

QString SettingsService::translateApiKey() const {
    return value(keys::trApiKey, QString()).toString().trimmed();
}

void SettingsService::setTranslateApiKey(const QString& key) {
    setValue(keys::trApiKey, key.trimmed());
}

QString SettingsService::translateAppId() const {
    return value(keys::trAppId, QString()).toString().trimmed();
}

void SettingsService::setTranslateAppId(const QString& appId) {
    setValue(keys::trAppId, appId.trimmed());
}

QString SettingsService::translateModel() const {
    return value(keys::trModel, QString()).toString().trimmed();
}

void SettingsService::setTranslateModel(const QString& model) {
    setValue(keys::trModel, model.trimmed());
}

QString SettingsService::translateTargetLang() const {
    const QString lang =
        value(keys::trTargetLang, QStringLiteral("zh")).toString();
    if (lang == QLatin1String("en") || lang == QLatin1String("ja")) {
        return lang;
    }
    return QStringLiteral("zh");
}

void SettingsService::setTranslateTargetLang(const QString& lang) {
    setValue(keys::trTargetLang, lang);
}

QJsonObject SettingsService::exportToJson() const {
    QJsonObject json;
    for (const QString& key : settings_.allKeys()) {
        json.insert(key, QJsonValue::fromVariant(settings_.value(key)));
    }
    return json;
}

void SettingsService::importFromJson(const QJsonObject& json) {
    for (auto it = json.begin(); it != json.end(); ++it) {
        setValue(it.key(), it.value().toVariant());
    }
    settings_.sync();
}

QString SettingsService::filePath() const {
    return settings_.fileName();
}

QVariant SettingsService::value(const QString& key, const QVariant& fallback) const {
    return settings_.value(key, fallback);
}

void SettingsService::setValue(const QString& key, const QVariant& newValue) {
    if (settings_.value(key) == newValue) {
        return;
    }
    settings_.setValue(key, newValue);
    emit changed(key);
}

} // namespace pixora
