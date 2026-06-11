#include "app/SettingsService.h"

#include <QJsonValue>

#include <algorithm>

namespace pixora {

namespace {
namespace keys {
const QString hotkeyCaptureRegion = QStringLiteral("hotkeys/captureRegion");
const QString hotkeyPinFromClipboard = QStringLiteral("hotkeys/pinFromClipboard");
const QString outputDir = QStringLiteral("output/dir");
const QString historyLimit = QStringLiteral("history/limit");
const QString fileNameTemplate = QStringLiteral("output/fileTemplate");
const QString outputFormat = QStringLiteral("output/format");
const QString outputQuality = QStringLiteral("output/quality");
const QString autoSave = QStringLiteral("output/autoSave");
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
