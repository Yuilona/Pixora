#pragma once

#include <QDialog>

class QCheckBox;
class QKeySequenceEdit;
class QLineEdit;

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

signals:
    void applied();

private:
    void apply();

    SettingsService& settings_;
    ISystemIntegration* system_;

    QKeySequenceEdit* captureEdit_ = nullptr;
    QKeySequenceEdit* scrollEdit_ = nullptr;
    QKeySequenceEdit* pinEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QCheckBox* autoStartCheck_ = nullptr;
};

} // namespace pixora
