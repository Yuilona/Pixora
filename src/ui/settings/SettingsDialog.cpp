#include "ui/settings/SettingsDialog.h"

#include "app/SettingsService.h"
#include "platform/interface/SystemIntegration.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
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
    setMinimumWidth(440);

    // 所有配置组统一为圆角卡片(样式见 Theme appStyleSheet 的 QGroupBox)
    auto makeCombo = [this] {
        auto* combo = new QComboBox(this);
        theme::roundComboPopup(combo); // 弹层透明化,圆角才完整
        return combo;
    };

    auto makeCard = [this](const QString& title, QFormLayout*& form) {
        auto* group = new QGroupBox(title, this);
        form = new QFormLayout(group);
        form->setHorizontalSpacing(14);
        form->setVerticalSpacing(10);
        form->setContentsMargins(12, 14, 12, 12);
        return group;
    };

    // —— 通用:热键 / 历史 / 自启 ——
    QFormLayout* generalForm = nullptr;
    auto* generalGroup = makeCard(QStringLiteral("通用"), generalForm);

    captureEdit_ = new QKeySequenceEdit(settings_.hotkeyCaptureRegion(), this);
    pinEdit_ = new QKeySequenceEdit(settings_.hotkeyPinFromClipboard(), this);
    // 全局热键只支持单组合键;默认可录 4 段序列("F1, A, B"),只会困惑
    captureEdit_->setMaximumSequenceLength(1);
    pinEdit_->setMaximumSequenceLength(1);
    generalForm->addRow(QStringLiteral("截图热键"), captureEdit_);
    generalForm->addRow(QStringLiteral("贴图热键"), pinEdit_);

    hotkeyWarning_ = new QLabel(
        QStringLiteral("标红的热键注册失败(可能已被其它程序占用),请更换后保存"), this);
    hotkeyWarning_->setStyleSheet(
        QStringLiteral("color:%1;").arg(theme::danger().name()));
    hotkeyWarning_->setWordWrap(true);
    hotkeyWarning_->hide();
    generalForm->addRow(QString(), hotkeyWarning_);

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

    historyLimitSpin_ = new QSpinBox(this);
    historyLimitSpin_->setRange(0, 100);
    historyLimitSpin_->setValue(settings_.historyLimit());
    historyLimitSpin_->setSpecialValueText(QStringLiteral("关闭"));
    historyLimitSpin_->setMaximumWidth(120); // 数字框不必占满整行
    generalForm->addRow(QStringLiteral("历史保留张数"), historyLimitSpin_);

    autoStartCheck_ = new QCheckBox(QStringLiteral("开机自动启动"), this);
    autoStartCheck_->setEnabled(system_ != nullptr);
    autoStartCheck_->setChecked(system_ && system_->isAutoStartEnabled());
    generalForm->addRow(QString(), autoStartCheck_);

    // —— 输出:目录 / 命名 / 格式 ——
    QFormLayout* outputForm = nullptr;
    auto* outputGroup = makeCard(QStringLiteral("输出"), outputForm);

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
    outputForm->addRow(QStringLiteral("保存目录"), dirRow);

    fileTemplateEdit_ = new QLineEdit(settings_.fileNameTemplate(), this);
    fileTemplateEdit_->setPlaceholderText(
        QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"));
    outputForm->addRow(QStringLiteral("文件名模板"), fileTemplateEdit_);

    auto* formatRow = new QHBoxLayout;
    formatCombo_ = makeCombo();
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
    outputForm->addRow(QStringLiteral("保存格式"), formatRow);

    autoSaveCheck_ = new QCheckBox(QStringLiteral("复制时自动保存到保存目录"), this);
    autoSaveCheck_->setChecked(settings_.autoSave());
    outputForm->addRow(QString(), autoSaveCheck_);

    // —— OCR 识别服务(提取文字 / 截图翻译共用)——
    QFormLayout* ocrForm = nullptr;
    auto* ocrGroup = makeCard(QStringLiteral("OCR 识别(提取文字 / 翻译)"), ocrForm);
    ocrProtocolCombo_ = makeCombo();
    ocrProtocolCombo_->addItem(QStringLiteral("OpenAI 兼容视觉模型"),
                               QStringLiteral("openai"));
    ocrProtocolCombo_->addItem(QStringLiteral("Umi-OCR 本地服务"),
                               QStringLiteral("umiocr"));
    ocrProtocolCombo_->setCurrentIndex(
        std::max(0, ocrProtocolCombo_->findData(settings_.ocrProtocol())));
    ocrUrlEdit_ = new QLineEdit(settings_.ocrBaseUrl(), this);
    ocrKeyEdit_ = new QLineEdit(settings_.ocrApiKey(), this);
    ocrKeyEdit_->setEchoMode(QLineEdit::Password);
    ocrModelEdit_ = new QLineEdit(settings_.ocrModel(), this);
    ocrModelEdit_->setPlaceholderText(QStringLiteral("如 qwen-vl-plus / glm-4v-flash"));
    ocrForm->addRow(QStringLiteral("协议"), ocrProtocolCombo_);
    ocrForm->addRow(QStringLiteral("接口地址"), ocrUrlEdit_);
    ocrForm->addRow(QStringLiteral("API Key"), ocrKeyEdit_);
    ocrForm->addRow(QStringLiteral("模型名"), ocrModelEdit_);
    auto syncOcrRows = [this, ocrForm] {
        const bool openai =
            ocrProtocolCombo_->currentData().toString() == QLatin1String("openai");
        ocrForm->setRowVisible(ocrKeyEdit_, openai);
        ocrForm->setRowVisible(ocrModelEdit_, openai);
        ocrUrlEdit_->setPlaceholderText(
            openai ? QStringLiteral("https://api.siliconflow.cn/v1")
                   : QStringLiteral("http://127.0.0.1:1224(留空用默认)"));
    };
    connect(ocrProtocolCombo_, &QComboBox::currentIndexChanged, this, syncOcrRows);
    syncOcrRows();

    // —— 翻译服务 ——
    QFormLayout* trForm = nullptr;
    auto* trGroup = makeCard(QStringLiteral("翻译服务(截图翻译)"), trForm);
    trProtocolCombo_ = makeCombo();
    trProtocolCombo_->addItem(QStringLiteral("OpenAI 兼容大模型"),
                              QStringLiteral("openai"));
    trProtocolCombo_->addItem(QStringLiteral("DeepL"), QStringLiteral("deepl"));
    trProtocolCombo_->addItem(QStringLiteral("百度翻译"), QStringLiteral("baidu"));
    trProtocolCombo_->setCurrentIndex(
        std::max(0, trProtocolCombo_->findData(settings_.translateProtocol())));
    trUrlEdit_ = new QLineEdit(settings_.translateBaseUrl(), this);
    trAppIdEdit_ = new QLineEdit(settings_.translateAppId(), this);
    trKeyEdit_ = new QLineEdit(settings_.translateApiKey(), this);
    trKeyEdit_->setEchoMode(QLineEdit::Password);
    trModelEdit_ = new QLineEdit(settings_.translateModel(), this);
    trModelEdit_->setPlaceholderText(QStringLiteral("如 deepseek-chat / qwen-turbo"));
    targetLangCombo_ = makeCombo();
    targetLangCombo_->addItem(QStringLiteral("中文"), QStringLiteral("zh"));
    targetLangCombo_->addItem(QStringLiteral("英文"), QStringLiteral("en"));
    targetLangCombo_->addItem(QStringLiteral("日文"), QStringLiteral("ja"));
    targetLangCombo_->setCurrentIndex(
        std::max(0, targetLangCombo_->findData(settings_.translateTargetLang())));
    trForm->addRow(QStringLiteral("协议"), trProtocolCombo_);
    trForm->addRow(QStringLiteral("接口地址"), trUrlEdit_);
    trForm->addRow(QStringLiteral("APP ID"), trAppIdEdit_);
    trForm->addRow(QStringLiteral("密钥"), trKeyEdit_);
    trForm->addRow(QStringLiteral("模型名"), trModelEdit_);
    trForm->addRow(QStringLiteral("目标语言"), targetLangCombo_);
    auto syncTrRows = [this, trForm] {
        const QString protocol = trProtocolCombo_->currentData().toString();
        trForm->setRowVisible(trUrlEdit_, protocol != QLatin1String("baidu"));
        trForm->setRowVisible(trAppIdEdit_, protocol == QLatin1String("baidu"));
        trForm->setRowVisible(trModelEdit_, protocol == QLatin1String("openai"));
        if (protocol == QLatin1String("openai")) {
            trUrlEdit_->setPlaceholderText(QStringLiteral("https://api.deepseek.com/v1"));
        } else if (protocol == QLatin1String("deepl")) {
            trUrlEdit_->setPlaceholderText(
                QStringLiteral("https://api-free.deepl.com(留空用默认)"));
        }
    };
    connect(trProtocolCombo_, &QComboBox::currentIndexChanged, this, syncTrRows);
    syncTrRows();

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Ok)->setStyleSheet(theme::primaryButtonStyle());
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(12);
    layout->addWidget(generalGroup);
    layout->addWidget(outputGroup);
    layout->addWidget(ocrGroup);
    layout->addWidget(trGroup);
    layout->addWidget(buttons);
}

void SettingsDialog::markHotkeyConflicts(bool captureFailed, bool pinFailed) {
    // 红框画在内部 QLineEdit 上(QKeySequenceEdit 本体不绘制边框)
    const QString style = QStringLiteral("QLineEdit { border: 1px solid %1; }")
                              .arg(theme::danger().name());
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
    settings_.setOcrProtocol(ocrProtocolCombo_->currentData().toString());
    settings_.setOcrBaseUrl(ocrUrlEdit_->text());
    settings_.setOcrApiKey(ocrKeyEdit_->text());
    settings_.setOcrModel(ocrModelEdit_->text());
    settings_.setTranslateProtocol(trProtocolCombo_->currentData().toString());
    settings_.setTranslateBaseUrl(trUrlEdit_->text());
    settings_.setTranslateAppId(trAppIdEdit_->text());
    settings_.setTranslateApiKey(trKeyEdit_->text());
    settings_.setTranslateModel(trModelEdit_->text());
    settings_.setTranslateTargetLang(targetLangCombo_->currentData().toString());
    if (system_) {
        system_->setAutoStart(autoStartCheck_->isChecked());
    }
    emit applied();
}

} // namespace pixora
