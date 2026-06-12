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
    setWindowTitle(tr("Settings - Pixora"));
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

    // —— 通用:语言 / 热键 / 历史 / 自启 ——
    QFormLayout* generalForm = nullptr;
    auto* generalGroup = makeCard(tr("General"), generalForm);

    // 语言名用各自母语显示,不随界面语言翻译
    languageCombo_ = makeCombo();
    languageCombo_->addItem(tr("Follow system"), QStringLiteral("auto"));
    languageCombo_->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    languageCombo_->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageCombo_->setCurrentIndex(
        std::max(0, languageCombo_->findData(settings_.language())));
    generalForm->addRow(tr("Language"), languageCombo_);

    captureEdit_ = new QKeySequenceEdit(settings_.hotkeyCaptureRegion(), this);
    pinEdit_ = new QKeySequenceEdit(settings_.hotkeyPinFromClipboard(), this);
    // 全局热键只支持单组合键;默认可录 4 段序列("F1, A, B"),只会困惑
    captureEdit_->setMaximumSequenceLength(1);
    pinEdit_->setMaximumSequenceLength(1);
    generalForm->addRow(tr("Capture hotkey"), captureEdit_);
    generalForm->addRow(tr("Pin hotkey"), pinEdit_);

    hotkeyWarning_ = new QLabel(
        tr("Hotkeys marked in red failed to register (possibly taken by another "
           "program); change them and save"),
        this);
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
    historyLimitSpin_->setSpecialValueText(tr("Off"));
    historyLimitSpin_->setMaximumWidth(120); // 数字框不必占满整行
    generalForm->addRow(tr("History size"), historyLimitSpin_);

    autoStartCheck_ = new QCheckBox(tr("Start at login"), this);
    autoStartCheck_->setEnabled(system_ != nullptr);
    autoStartCheck_->setChecked(system_ && system_->isAutoStartEnabled());
    generalForm->addRow(QString(), autoStartCheck_);

    // —— 输出:目录 / 命名 / 格式 ——
    QFormLayout* outputForm = nullptr;
    auto* outputGroup = makeCard(tr("Output"), outputForm);

    auto* dirRow = new QHBoxLayout;
    outputDirEdit_ = new QLineEdit(settings_.outputDir(), this);
    outputDirEdit_->setPlaceholderText(
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    auto* browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Choose save folder"),
            outputDirEdit_->text().isEmpty() ? outputDirEdit_->placeholderText()
                                             : outputDirEdit_->text());
        if (!dir.isEmpty()) {
            outputDirEdit_->setText(dir);
        }
    });
    dirRow->addWidget(outputDirEdit_, 1);
    dirRow->addWidget(browseBtn);
    outputForm->addRow(tr("Save folder"), dirRow);

    fileTemplateEdit_ = new QLineEdit(settings_.fileNameTemplate(), this);
    fileTemplateEdit_->setPlaceholderText(
        QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"));
    outputForm->addRow(tr("Filename template"), fileTemplateEdit_);

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
    formatRow->addWidget(new QLabel(tr("Quality"), this));
    formatRow->addWidget(qualitySpin_);
    outputForm->addRow(tr("Format"), formatRow);

    autoSaveCheck_ = new QCheckBox(tr("Also save to the folder when copying"), this);
    autoSaveCheck_->setChecked(settings_.autoSave());
    outputForm->addRow(QString(), autoSaveCheck_);

    // —— OCR 识别服务(提取文字 / 截图翻译共用)——
    QFormLayout* ocrForm = nullptr;
    auto* ocrGroup = makeCard(tr("OCR (extract text / translate)"), ocrForm);
    ocrProtocolCombo_ = makeCombo();
    ocrProtocolCombo_->addItem(tr("OpenAI-compatible vision model"),
                               QStringLiteral("openai"));
    ocrProtocolCombo_->addItem(tr("Umi-OCR local service"),
                               QStringLiteral("umiocr"));
    ocrProtocolCombo_->setCurrentIndex(
        std::max(0, ocrProtocolCombo_->findData(settings_.ocrProtocol())));
    ocrUrlEdit_ = new QLineEdit(settings_.ocrBaseUrl(), this);
    ocrKeyEdit_ = new QLineEdit(settings_.ocrApiKey(), this);
    ocrKeyEdit_->setEchoMode(QLineEdit::Password);
    ocrModelEdit_ = new QLineEdit(settings_.ocrModel(), this);
    ocrModelEdit_->setPlaceholderText(
        tr("e.g. qwen-vl-plus / glm-4v-flash"));
    ocrForm->addRow(tr("Protocol"), ocrProtocolCombo_);
    ocrForm->addRow(tr("Endpoint"), ocrUrlEdit_);
    ocrForm->addRow(QStringLiteral("API Key"), ocrKeyEdit_);
    ocrForm->addRow(tr("Model"), ocrModelEdit_);
    auto syncOcrRows = [this, ocrForm] {
        const bool openai =
            ocrProtocolCombo_->currentData().toString() == QLatin1String("openai");
        ocrForm->setRowVisible(ocrKeyEdit_, openai);
        ocrForm->setRowVisible(ocrModelEdit_, openai);
        ocrUrlEdit_->setPlaceholderText(
            openai ? QStringLiteral("https://api.siliconflow.cn/v1")
                   : tr("http://127.0.0.1:1224 (leave empty for default)"));
    };
    connect(ocrProtocolCombo_, &QComboBox::currentIndexChanged, this, syncOcrRows);
    syncOcrRows();

    // —— 翻译服务 ——
    QFormLayout* trForm = nullptr;
    auto* trGroup = makeCard(tr("Translation (screenshot translate)"), trForm);
    trProtocolCombo_ = makeCombo();
    trProtocolCombo_->addItem(tr("OpenAI-compatible chat model"),
                              QStringLiteral("openai"));
    trProtocolCombo_->addItem(QStringLiteral("DeepL"), QStringLiteral("deepl"));
    trProtocolCombo_->addItem(tr("DeepLX (self-hosted)"), QStringLiteral("deeplx"));
    trProtocolCombo_->addItem(tr("Baidu Translate"), QStringLiteral("baidu"));
    trProtocolCombo_->setCurrentIndex(
        std::max(0, trProtocolCombo_->findData(settings_.translateProtocol())));
    trUrlEdit_ = new QLineEdit(settings_.translateBaseUrl(), this);
    trAppIdEdit_ = new QLineEdit(settings_.translateAppId(), this);
    trKeyEdit_ = new QLineEdit(settings_.translateApiKey(), this);
    trKeyEdit_->setEchoMode(QLineEdit::Password);
    trModelEdit_ = new QLineEdit(settings_.translateModel(), this);
    trModelEdit_->setPlaceholderText(tr("e.g. deepseek-chat / qwen-turbo"));
    targetLangCombo_ = makeCombo();
    targetLangCombo_->addItem(tr("Chinese"), QStringLiteral("zh"));
    targetLangCombo_->addItem(tr("English"), QStringLiteral("en"));
    targetLangCombo_->addItem(tr("Japanese"), QStringLiteral("ja"));
    targetLangCombo_->setCurrentIndex(
        std::max(0, targetLangCombo_->findData(settings_.translateTargetLang())));
    trForm->addRow(tr("Protocol"), trProtocolCombo_);
    trForm->addRow(tr("Endpoint"), trUrlEdit_);
    trForm->addRow(QStringLiteral("APP ID"), trAppIdEdit_);
    trForm->addRow(tr("Secret key"), trKeyEdit_);
    trForm->addRow(tr("Model"), trModelEdit_);
    trForm->addRow(tr("Target language"), targetLangCombo_);
    auto syncTrRows = [this, trForm] {
        const QString protocol = trProtocolCombo_->currentData().toString();
        trForm->setRowVisible(trUrlEdit_, protocol != QLatin1String("baidu"));
        trForm->setRowVisible(trAppIdEdit_, protocol == QLatin1String("baidu"));
        trForm->setRowVisible(trModelEdit_, protocol == QLatin1String("openai"));
        if (protocol == QLatin1String("openai")) {
            trUrlEdit_->setPlaceholderText(QStringLiteral("https://api.deepseek.com/v1"));
        } else if (protocol == QLatin1String("deepl")) {
            // 留空时按 key 后缀自动选 api-free/api(免费 key 以 :fx 结尾)
            trUrlEdit_->setPlaceholderText(
                tr("Leave empty to auto-select by API key"));
        } else if (protocol == QLatin1String("deeplx")) {
            trUrlEdit_->setPlaceholderText(
                tr("http://127.0.0.1:1188 (leave empty for default)"));
        }
        // DeepLX 的令牌是可选项,其余协议密钥必填
        trKeyEdit_->setPlaceholderText(protocol == QLatin1String("deeplx")
                                           ? tr("Access token (optional)")
                                           : QString());
    };
    connect(trProtocolCombo_, &QComboBox::currentIndexChanged, this, syncTrRows);
    syncTrRows();

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Ok)->setStyleSheet(theme::primaryButtonStyle());
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
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
    settings_.setLanguage(languageCombo_->currentData().toString());
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
