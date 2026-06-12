#include "ui/settings/SettingsDialog.h"

#include "app/SettingsService.h"
#include "platform/interface/SystemIntegration.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>

#include <algorithm>

namespace pixora {

SettingsDialog::SettingsDialog(SettingsService& settings, ISystemIntegration* system,
                               QWidget* parent)
    : QDialog(parent), settings_(settings), system_(system) {
    setWindowTitle(QStringLiteral("设置 — Pixora"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(380);

    auto* form = new QFormLayout;

    captureEdit_ = new QKeySequenceEdit(settings_.hotkeyCaptureRegion(), this);
    pinEdit_ = new QKeySequenceEdit(settings_.hotkeyPinFromClipboard(), this);
    form->addRow(QStringLiteral("截图热键"), captureEdit_);
    form->addRow(QStringLiteral("贴图热键"), pinEdit_);

    hotkeyWarning_ = new QLabel(
        QStringLiteral("标红的热键注册失败(可能已被其它程序占用),请更换后保存"), this);
    hotkeyWarning_->setStyleSheet(QStringLiteral("color:#d93025;"));
    hotkeyWarning_->setWordWrap(true);
    hotkeyWarning_->hide();
    form->addRow(QString(), hotkeyWarning_);

    auto clearConflict = [this](QKeySequenceEdit* edit) {
        edit->setStyleSheet(QString());
        if (captureEdit_->styleSheet().isEmpty() && pinEdit_->styleSheet().isEmpty()) {
            hotkeyWarning_->hide();
        }
    };
    connect(captureEdit_, &QKeySequenceEdit::keySequenceChanged, this,
            [this, clearConflict] { clearConflict(captureEdit_); });
    connect(pinEdit_, &QKeySequenceEdit::keySequenceChanged, this,
            [this, clearConflict] { clearConflict(pinEdit_); });

    auto* dirRow = new QHBoxLayout;
    outputDirEdit_ = new QLineEdit(settings_.outputDir(), this);
    outputDirEdit_->setPlaceholderText(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    auto* browseBtn = new QPushButton(QStringLiteral("浏览…"), this);
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择保存目录"),
            outputDirEdit_->text().isEmpty() ? outputDirEdit_->placeholderText()
                                             : outputDirEdit_->text());
        if (!dir.isEmpty()) {
            outputDirEdit_->setText(dir);
        }
    });
    dirRow->addWidget(outputDirEdit_, 1);
    dirRow->addWidget(browseBtn);
    form->addRow(QStringLiteral("保存目录"), dirRow);

    fileTemplateEdit_ = new QLineEdit(settings_.fileNameTemplate(), this);
    fileTemplateEdit_->setPlaceholderText(
        QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"));
    form->addRow(QStringLiteral("文件名模板"), fileTemplateEdit_);

    auto* formatRow = new QHBoxLayout;
    formatCombo_ = new QComboBox(this);
    formatCombo_->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    formatCombo_->addItem(QStringLiteral("JPEG"), QStringLiteral("jpg"));
    formatCombo_->setCurrentIndex(
        std::max(0, formatCombo_->findData(settings_.outputFormat())));
    qualitySpin_ = new QSpinBox(this);
    qualitySpin_->setRange(10, 100);
    qualitySpin_->setSuffix(QStringLiteral("%"));
    qualitySpin_->setValue(settings_.outputQuality());
    auto syncQualityEnabled = [this] {
        // PNG 无损,质量项只对 jpg/webp 生效
        qualitySpin_->setEnabled(formatCombo_->currentData().toString() !=
                                 QLatin1String("png"));
    };
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, syncQualityEnabled);
    syncQualityEnabled();
    formatRow->addWidget(formatCombo_, 1);
    formatRow->addWidget(new QLabel(QStringLiteral("质量"), this));
    formatRow->addWidget(qualitySpin_);
    form->addRow(QStringLiteral("保存格式"), formatRow);

    autoSaveCheck_ = new QCheckBox(QStringLiteral("复制时自动保存到保存目录"), this);
    autoSaveCheck_->setChecked(settings_.autoSave());
    form->addRow(QString(), autoSaveCheck_);

    historyLimitSpin_ = new QSpinBox(this);
    historyLimitSpin_->setRange(0, 100);
    historyLimitSpin_->setValue(settings_.historyLimit());
    historyLimitSpin_->setSpecialValueText(QStringLiteral("关闭"));
    form->addRow(QStringLiteral("历史保留张数"), historyLimitSpin_);

    autoStartCheck_ = new QCheckBox(QStringLiteral("开机自动启动"), this);
    autoStartCheck_->setEnabled(system_ != nullptr);
    autoStartCheck_->setChecked(system_ && system_->isAutoStartEnabled());
    form->addRow(QString(), autoStartCheck_);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SettingsDialog::markHotkeyConflicts(bool captureFailed, bool pinFailed) {
    // 红框画在内部 QLineEdit 上(QKeySequenceEdit 本体不绘制边框)
    const QString style = QStringLiteral("QLineEdit { border: 1px solid #d93025; }");
    captureEdit_->setStyleSheet(captureFailed ? style : QString());
    pinEdit_->setStyleSheet(pinFailed ? style : QString());
    hotkeyWarning_->setVisible(captureFailed || pinFailed);
}

void SettingsDialog::apply() {
    // 空热键保持原值(避免误清空导致功能不可达)
    if (!captureEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyCaptureRegion(captureEdit_->keySequence());
    }
    if (!pinEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyPinFromClipboard(pinEdit_->keySequence());
    }
    settings_.setOutputDir(outputDirEdit_->text().trimmed());
    settings_.setFileNameTemplate(fileTemplateEdit_->text().trimmed());
    settings_.setOutputFormat(formatCombo_->currentData().toString());
    settings_.setOutputQuality(qualitySpin_->value());
    settings_.setAutoSave(autoSaveCheck_->isChecked());
    settings_.setHistoryLimit(historyLimitSpin_->value());
    if (system_) {
        system_->setAutoStart(autoStartCheck_->isChecked());
    }
    emit applied();
}

} // namespace pixora
