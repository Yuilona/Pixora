#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace pixora {

class ISystemIntegration;
class SettingsService;

// 设置对话框:全局热键 / 保存目录 / 开机自启。
// 确定时写入 SettingsService 并发 applied(),由 App 层触发热键重注册。
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(SettingsService& settings, ISystemIntegration* system,
                   QWidget* parent = nullptr);

    // 标红注册失败的热键并显示提示;用户改键后自动清除
    void markHotkeyConflicts(bool captureFailed, bool pinFailed, bool repeatFailed);

signals:
    void applied();

private:
    void apply();

    SettingsService& settings_;
    ISystemIntegration* system_;

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
    QCheckBox* updateCheck_ = nullptr;

    // OCR / 翻译服务(截图翻译)
    QComboBox* ocrProtocolCombo_ = nullptr;
    QLineEdit* ocrUrlEdit_ = nullptr;
    QLineEdit* ocrKeyEdit_ = nullptr;
    QLineEdit* ocrModelEdit_ = nullptr;
    QComboBox* trProtocolCombo_ = nullptr;
    QLineEdit* trUrlEdit_ = nullptr;
    QLineEdit* trAppIdEdit_ = nullptr;
    QLineEdit* trKeyEdit_ = nullptr;
    QLineEdit* trModelEdit_ = nullptr;
    QComboBox* targetLangCombo_ = nullptr;
};

} // namespace pixora
