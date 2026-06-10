#include "app/SettingsService.h"

#include <QJsonValue>

namespace pixora {

namespace {
namespace keys {
const QString hotkeyCaptureRegion = QStringLiteral("hotkeys/captureRegion");
const QString hotkeyScrollCapture = QStringLiteral("hotkeys/scrollCapture");
const QString hotkeyPinFromClipboard = QStringLiteral("hotkeys/pinFromClipboard");
} // namespace keys

// 默认热键对标 Snipaste:F1 截图、F3 贴图;F2 为长截图。
const QString defaultCaptureRegion = QStringLiteral("F1");
const QString defaultScrollCapture = QStringLiteral("F2");
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

QKeySequence SettingsService::hotkeyScrollCapture() const {
    return toKeySequence(value(keys::hotkeyScrollCapture, defaultScrollCapture));
}

QKeySequence SettingsService::hotkeyPinFromClipboard() const {
    return toKeySequence(value(keys::hotkeyPinFromClipboard, defaultPinFromClipboard));
}

void SettingsService::setHotkeyCaptureRegion(const QKeySequence& seq) {
    setValue(keys::hotkeyCaptureRegion, seq.toString(QKeySequence::PortableText));
}

void SettingsService::setHotkeyScrollCapture(const QKeySequence& seq) {
    setValue(keys::hotkeyScrollCapture, seq.toString(QKeySequence::PortableText));
}

void SettingsService::setHotkeyPinFromClipboard(const QKeySequence& seq) {
    setValue(keys::hotkeyPinFromClipboard, seq.toString(QKeySequence::PortableText));
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
