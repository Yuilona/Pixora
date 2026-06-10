#pragma once

#include <QJsonObject>
#include <QKeySequence>
#include <QObject>
#include <QSettings>
#include <QString>

namespace pixora {

// 强类型配置访问层:所有配置项经具名 getter/setter 读写,
// 字符串 key 不外泄(见 ARCHITECTURE §6)。存储为 INI,支持 JSON 导入导出。
class SettingsService : public QObject {
    Q_OBJECT
public:
    // iniPath 为空时使用平台标准配置目录;测试中注入临时文件路径。
    explicit SettingsService(const QString& iniPath = {}, QObject* parent = nullptr);

    QKeySequence hotkeyCaptureRegion() const;
    QKeySequence hotkeyScrollCapture() const;
    QKeySequence hotkeyPinFromClipboard() const;
    void setHotkeyCaptureRegion(const QKeySequence& seq);
    void setHotkeyScrollCapture(const QKeySequence& seq);
    void setHotkeyPinFromClipboard(const QKeySequence& seq);

    QJsonObject exportToJson() const;
    void importFromJson(const QJsonObject& json);

    QString filePath() const;

signals:
    void changed(const QString& key);

private:
    QVariant value(const QString& key, const QVariant& fallback) const;
    void setValue(const QString& key, const QVariant& newValue);

    mutable QSettings settings_;
};

} // namespace pixora
