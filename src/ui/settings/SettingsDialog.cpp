#include "ui/settings/SettingsDialog.h"

#include "app/SettingsService.h"
#include "platform/interface/SystemIntegration.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>

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

void SettingsDialog::apply() {
    // 空热键保持原值(避免误清空导致功能不可达)
    if (!captureEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyCaptureRegion(captureEdit_->keySequence());
    }
    if (!pinEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyPinFromClipboard(pinEdit_->keySequence());
    }
    settings_.setOutputDir(outputDirEdit_->text().trimmed());
    if (system_) {
        system_->setAutoStart(autoStartCheck_->isChecked());
    }
    emit applied();
}

} // namespace pixora
