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

    // 界面语言:"auto"(跟随系统)| "zh_CN" | "en"
    QString language() const;
    void setLanguage(const QString& lang);

    QKeySequence hotkeyCaptureRegion() const;
    QKeySequence hotkeyPinFromClipboard() const;
    void setHotkeyCaptureRegion(const QKeySequence& seq);
    void setHotkeyPinFromClipboard(const QKeySequence& seq);

    // 截图保存目录;空串表示使用系统图片目录
    QString outputDir() const;
    void setOutputDir(const QString& dir);

    // 截图历史保留张数;0 = 关闭历史
    int historyLimit() const;
    void setHistoryLimit(int count);

    // 文件名模板:{yyyy}{MM}{dd}{HH}{mm}{ss} 占位符(见 FileNameTemplate.h)
    QString fileNameTemplate() const;
    void setFileNameTemplate(const QString& tmpl);

    // 保存格式 "png"/"jpg"/"webp" 与质量(jpg/webp 用,1-100)
    QString outputFormat() const;
    void setOutputFormat(const QString& format);
    int outputQuality() const;
    void setOutputQuality(int quality);

    // 复制出口同时自动保存到保存目录
    bool autoSave() const;
    void setAutoSave(bool enabled);

    // —— OCR 服务(提取文字/截图翻译)——
    QString ocrProtocol() const; // "openai" | "umiocr"
    void setOcrProtocol(const QString& protocol);
    QString ocrBaseUrl() const;
    void setOcrBaseUrl(const QString& url);
    QString ocrApiKey() const;
    void setOcrApiKey(const QString& key);
    QString ocrModel() const;
    void setOcrModel(const QString& model);

    // —— 翻译服务(截图翻译)——
    QString translateProtocol() const; // "openai" | "deepl" | "baidu"
    void setTranslateProtocol(const QString& protocol);
    QString translateBaseUrl() const;
    void setTranslateBaseUrl(const QString& url);
    QString translateApiKey() const;
    void setTranslateApiKey(const QString& key);
    QString translateAppId() const; // 仅百度
    void setTranslateAppId(const QString& appId);
    QString translateModel() const; // 仅 openai
    void setTranslateModel(const QString& model);
    QString translateTargetLang() const; // "zh" | "en" | "ja"
    void setTranslateTargetLang(const QString& lang);

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
