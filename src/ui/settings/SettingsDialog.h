#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace pixora {

class ISystemIntegration;
class SettingsService;

// 设置对话框:左侧导航 + 右侧分页(通用 / 快捷键 / 输出 / 文字识别与翻译 / 关于),
// 窗口定高,页内超出时滚动——不随设置项增多而长出屏幕。
// 确定时写入 SettingsService 并发 applied(),由 App 层触发热键重注册。
// "测试连接""检查更新"只发请求信号(本窗不依赖编排型服务),结果经 show* 回填。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(SettingsService& settings, ISystemIntegration* system,
                   QWidget* parent = nullptr);

    // 标红注册失败的热键并显示提示(有冲突时直接打开快捷键页);用户改键后自动清除
    void markHotkeyConflicts(bool captureFailed, bool pinFailed, bool repeatFailed);

    void showOcrTestResult(bool ok, const QString& detail);
    void showTranslateTestResult(bool ok, const QString& detail);
    // ok=false 时 version 为失败原因;newer=true 时 url 为下载页
    void showUpdateCheckResult(bool ok, bool newer, const QString& version,
                               const QString& url);

signals:
    void applied();
    // 用表单当前值(可未保存)测试服务
    void ocrTestRequested(const QString& protocol, const QString& baseUrl,
                          const QString& apiKey, const QString& model);
    void translateTestRequested(const QString& protocol, const QString& baseUrl,
                                const QString& apiKey, const QString& appId,
                                const QString& model, const QString& targetLang);
    void updateCheckRequested();

private:
    QWidget* buildGeneralPage();
    QWidget* buildHotkeysPage();
    QWidget* buildOutputPage();
    QWidget* buildTextPage();
    QWidget* buildAboutPage();
    QGroupBox* makeCard(const QString& title, QFormLayout*& form);
    QComboBox* makeCombo();
    // scrollable:页内容可能超出窗口高(仅文字识别页);其余页在最小窗口高度下
    // 也放得下,不套滚动区——带折行标签的页套上可伸缩滚动区会误出滚动条
    void addPage(const char* iconName, const QString& title, QWidget* content,
                 bool scrollable = false);
    void apply();

    SettingsService& settings_;
    ISystemIntegration* system_;

    QListWidget* nav_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLabel* pageTitle_ = nullptr;
    QStringList pageTitles_;

    QComboBox* languageCombo_ = nullptr;
    QKeySequenceEdit* captureEdit_ = nullptr;
    QKeySequenceEdit* pinEdit_ = nullptr;
    QKeySequenceEdit* repeatEdit_ = nullptr;
    QLabel* hotkeyWarning_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* fileTemplateEdit_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QSpinBox* qualitySpin_ = nullptr;
    QCheckBox* autoSaveCheck_ = nullptr;
    QSpinBox* historyLimitSpin_ = nullptr;
    QCheckBox* autoStartCheck_ = nullptr;
    bool autoStartInitial_ = false; // 打开时的实际状态,保存时据此判断是否改动
    QCheckBox* updateCheck_ = nullptr;

    // OCR / 翻译服务(截图翻译)
    QComboBox* ocrProtocolCombo_ = nullptr;
    QLineEdit* ocrUrlEdit_ = nullptr;
    QLineEdit* ocrKeyEdit_ = nullptr;
    QLineEdit* ocrModelEdit_ = nullptr;
    QPushButton* ocrTestBtn_ = nullptr;
    QLabel* ocrTestResult_ = nullptr;
    QComboBox* trProtocolCombo_ = nullptr;
    QLineEdit* trUrlEdit_ = nullptr;
    QLineEdit* trAppIdEdit_ = nullptr;
    QLineEdit* trKeyEdit_ = nullptr;
    QLineEdit* trModelEdit_ = nullptr;
    QComboBox* targetLangCombo_ = nullptr;
    QPushButton* trTestBtn_ = nullptr;
    QLabel* trTestResult_ = nullptr;

    QPushButton* updateBtn_ = nullptr;
    QLabel* updateResult_ = nullptr;
};

} // namespace pixora
