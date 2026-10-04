#include "ui/settings/SettingsDialog.h"

#include "app/FileNameTemplate.h"
#include "app/SettingsService.h"
#include "platform/interface/SystemIntegration.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace pixora {

namespace {

const QString kHomepage = QStringLiteral("https://github.com/Yuilona/Pixora");

// 导航栏样式:浅灰侧栏,选中项为白色浮起块(与右侧白卡片同色呼应)
QString navStyleSheet() {
    return QStringLiteral(
        "QWidget#sidebar { background: #EEF0F4; border-right: 1px solid #E3E6EB; }"
        "QListWidget#settingsNav { background: transparent; border: none; outline: 0; }"
        "QListWidget#settingsNav::item { color: #3A4150; padding: 8px 10px;"
        "  margin: 1px 0; border-radius: 6px; border: 1px solid transparent; }"
        "QListWidget#settingsNav::item:hover { background: #E5E8ED; }"
        "QListWidget#settingsNav::item:selected { background: #FFFFFF; color: #1F2329;"
        "  border: 1px solid #E3E6EB; }");
}

enum class ResultTone { Pending, Ok, Error };

void showResultText(QLabel* label, ResultTone tone, const QString& text) {
    const QColor color = tone == ResultTone::Ok      ? theme::successOnLight()
                         : tone == ResultTone::Error ? theme::danger()
                                                     : QColor(0x8B, 0x91, 0x9C);
    label->setStyleSheet(QStringLiteral("color:%1;").arg(color.name()));
    label->setText(text);
}

QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color:%1;").arg(theme::textFaint().name()));
    return label;
}

} // namespace

SettingsDialog::SettingsDialog(SettingsService& settings, ISystemIntegration* system,
                               QWidget* parent)
    : QDialog(parent), settings_(settings), system_(system) {
    setWindowTitle(tr("Settings - Pixora"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(780, 540);
    setMinimumSize(700, 460);

    // —— 左侧导航 ——
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(184);
    sidebar->setStyleSheet(navStyleSheet());
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(12, 16, 12, 12);
    sideLayout->setSpacing(12);

    auto* brand = new QHBoxLayout;
    auto* logo = new QLabel(sidebar);
    QPixmap logoPm(QStringLiteral(":/icons/pixora-256.png"));
    logoPm.setDevicePixelRatio(logoPm.width() / 24.0);
    logo->setPixmap(logoPm);
    auto* brandName = new QLabel(QStringLiteral("Pixora"), sidebar);
    brandName->setStyleSheet(QStringLiteral("color:#1F2329; font-size:15px; font-weight:600;"));
    brand->addWidget(logo);
    brand->addSpacing(6);
    brand->addWidget(brandName);
    brand->addStretch();
    sideLayout->addLayout(brand);

    nav_ = new QListWidget(sidebar);
    nav_->setObjectName(QStringLiteral("settingsNav"));
    nav_->setIconSize(QSize(18, 18));
    nav_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideLayout->addWidget(nav_, 1);

    // —— 右侧:页标题 + 分页 + 底部按钮 ——
    pageTitle_ = new QLabel(this);
    pageTitle_->setStyleSheet(QStringLiteral("color:#1F2329; font-size:18px; font-weight:600;"));
    pages_ = new QStackedWidget(this);

    addPage("settings", tr("General"), buildGeneralPage());
    addPage("keyboard", tr("Hotkeys"), buildHotkeysPage());
    addPage("image", tr("Output"), buildOutputPage());
    addPage("scan_text", tr("Text recognition"), buildTextPage(), /*scrollable=*/true);
    addPage("info", tr("About"), buildAboutPage());
    connect(nav_, &QListWidget::currentRowChanged, this, [this](int row) {
        pages_->setCurrentIndex(row);
        pageTitle_->setText(pageTitles_.value(row));
    });
    nav_->setCurrentRow(0);

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

    auto* footerLine = new QWidget(this);
    footerLine->setFixedHeight(1);
    footerLine->setAttribute(Qt::WA_StyledBackground);
    footerLine->setStyleSheet(QStringLiteral("background:#E3E6EB;"));

    auto* right = new QVBoxLayout;
    right->setContentsMargins(24, 18, 20, 14);
    right->setSpacing(14);
    right->addWidget(pageTitle_);
    right->addWidget(pages_, 1);
    right->addWidget(footerLine);
    right->addWidget(buttons);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(sidebar);
    root->addLayout(right, 1);
}

QComboBox* SettingsDialog::makeCombo() {
    auto* combo = new QComboBox(this);
    theme::roundComboPopup(combo); // 弹层透明化,圆角才完整
    return combo;
}

QGroupBox* SettingsDialog::makeCard(const QString& title, QFormLayout*& form) {
    // 所有配置组统一为圆角卡片(样式见 Theme appStyleSheet 的 QGroupBox)
    auto* group = new QGroupBox(title, this);
    form = new QFormLayout(group);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setContentsMargins(12, 14, 12, 12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return group;
}

void SettingsDialog::addPage(const char* iconName, const QString& title, QWidget* content,
                             bool scrollable) {
    if (scrollable) {
        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // 给滚动条让出位置,卡片右缘不被压住
        content->layout()->setContentsMargins(0, 0, 14, 0);
        scroll->setWidget(content);
        pages_->addWidget(scroll);
    } else {
        pages_->addWidget(content);
    }
    pageTitles_ << title;
    new QListWidgetItem(icons::lightIcon(iconName), title, nav_);
}

QWidget* SettingsDialog::buildGeneralPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QFormLayout* basicForm = nullptr;
    auto* basicCard = makeCard(tr("General"), basicForm);
    // 语言名用各自母语显示,不随界面语言翻译
    languageCombo_ = makeCombo();
    languageCombo_->addItem(tr("Follow system"), QStringLiteral("auto"));
    languageCombo_->addItem(QStringLiteral("简体中文"), QStringLiteral("zh_CN"));
    languageCombo_->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageCombo_->setCurrentIndex(
        std::max(0, languageCombo_->findData(settings_.language())));
    basicForm->addRow(tr("Language"), languageCombo_);

    historyLimitSpin_ = new QSpinBox(this);
    historyLimitSpin_->setRange(0, 100);
    historyLimitSpin_->setValue(settings_.historyLimit());
    historyLimitSpin_->setSpecialValueText(tr("Off"));
    historyLimitSpin_->setMaximumWidth(120); // 数字框不必占满整行
    basicForm->addRow(tr("History size"), historyLimitSpin_);
    layout->addWidget(basicCard);

    QFormLayout* startupForm = nullptr;
    auto* startupCard = makeCard(tr("Startup"), startupForm);
    autoStartCheck_ = new QCheckBox(tr("Start at login"), this);
    autoStartCheck_->setEnabled(system_ != nullptr);
    const AutoStartStatus autoStart = system_ ? system_->autoStartStatus() : AutoStartStatus{};
    autoStartInitial_ = autoStart.state == AutoStartState::Enabled;
    autoStartCheck_->setChecked(autoStartInitial_);
    startupForm->addRow(autoStartCheck_);

    // 有自启项却显示未勾选时说明原因,免得用户以为"勾了又没了"。
    // 路径分隔符后插零宽空格,长路径才能折行而不撑宽窗口
    const QString breakableSep = QString(QLatin1Char('\\')) + QChar(0x200B);
    const QString target =
        QString(autoStart.target).replace(QLatin1Char('\\'), breakableSep);
    QString autoStartNote;
    switch (autoStart.state) {
    case AutoStartState::DisabledBySystem:
        autoStartNote = tr("Turned off in Windows startup settings (Task Manager). "
                           "Check to turn it back on.");
        break;
    case AutoStartState::OtherCopy:
        autoStartNote = tr("Currently starts another copy of Pixora: %1. "
                           "Check to start this copy instead.")
                            .arg(target);
        break;
    case AutoStartState::StalePath:
        autoStartNote = tr("The startup entry points to a missing file: %1. "
                           "Check to repair it.")
                            .arg(target);
        break;
    case AutoStartState::Off:
    case AutoStartState::Enabled:
        break;
    }
    if (!autoStartNote.isEmpty()) {
        startupForm->addRow(hintLabel(autoStartNote, this));
    }

    updateCheck_ = new QCheckBox(tr("Check for updates at startup"), this);
    updateCheck_->setChecked(settings_.checkUpdates());
    startupForm->addRow(updateCheck_);
    layout->addWidget(startupCard);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildHotkeysPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QFormLayout* form = nullptr;
    auto* card = makeCard(tr("Global hotkeys"), form);
    captureEdit_ = new QKeySequenceEdit(settings_.hotkeyCaptureRegion(), this);
    pinEdit_ = new QKeySequenceEdit(settings_.hotkeyPinFromClipboard(), this);
    repeatEdit_ = new QKeySequenceEdit(settings_.hotkeyRepeatLastRegion(), this);
    // 全局热键只支持单组合键;默认可录 4 段序列("F1, A, B"),只会困惑
    for (QKeySequenceEdit* edit : {captureEdit_, pinEdit_, repeatEdit_}) {
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        // 占位文字由内部 QLineEdit 显示;Qt 自带文案未随包翻译,自己给
        if (auto* line = edit->findChild<QLineEdit*>()) {
            line->setPlaceholderText(tr("Press a key combination"));
        }
    }
    form->addRow(tr("Capture hotkey"), captureEdit_);
    form->addRow(tr("Pin hotkey"), pinEdit_);
    // 默认不绑定;清空即解除绑定(输入框自带清除按钮)
    form->addRow(tr("Repeat last region"), repeatEdit_);

    hotkeyWarning_ = new QLabel(
        tr("Hotkeys marked in red failed to register (possibly taken by another "
           "program); change them and save"),
        this);
    hotkeyWarning_->setStyleSheet(
        QStringLiteral("color:%1;").arg(theme::danger().name()));
    hotkeyWarning_->setWordWrap(true);
    hotkeyWarning_->hide();
    form->addRow(hotkeyWarning_);
    form->addRow(hintLabel(tr("Click a box and press the new key combination. "
                              "\"Repeat last region\" is unbound by default; clear it "
                              "to unbind."),
                           this));

    auto clearConflict = [this](QKeySequenceEdit* edit) {
        edit->setStyleSheet(QString());
        if (captureEdit_->styleSheet().isEmpty() && pinEdit_->styleSheet().isEmpty() &&
            repeatEdit_->styleSheet().isEmpty()) {
            hotkeyWarning_->hide();
        }
    };
    connect(captureEdit_, &QKeySequenceEdit::keySequenceChanged, this,
            [this, clearConflict] { clearConflict(captureEdit_); });
    connect(pinEdit_, &QKeySequenceEdit::keySequenceChanged, this,
            [this, clearConflict] { clearConflict(pinEdit_); });
    connect(repeatEdit_, &QKeySequenceEdit::keySequenceChanged, this,
            [this, clearConflict] { clearConflict(repeatEdit_); });

    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildOutputPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QFormLayout* form = nullptr;
    auto* card = makeCard(tr("Output"), form);

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
    form->addRow(tr("Save folder"), dirRow);

    fileTemplateEdit_ = new QLineEdit(settings_.fileNameTemplate(), this);
    fileTemplateEdit_->setPlaceholderText(
        QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"));
    form->addRow(tr("Filename template"), fileTemplateEdit_);

    // 模板实时预览:所见即所得,免得存下来才发现格式不对
    auto* templatePreview = hintLabel(QString(), this);
    form->addRow(QString(), templatePreview);

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
    formatRow->addWidget(formatCombo_, 1);
    formatRow->addWidget(new QLabel(tr("Quality"), this));
    formatRow->addWidget(qualitySpin_);
    form->addRow(tr("Format"), formatRow);

    const auto syncOutput = [this, templatePreview] {
        // PNG 无损,质量项只对 jpg 生效
        const QString ext = formatCombo_->currentData().toString();
        qualitySpin_->setEnabled(ext != QLatin1String("png"));
        templatePreview->setText(
            tr("Example: %1")
                .arg(expandFileNameTemplate(fileTemplateEdit_->text(),
                                            QDateTime::currentDateTime()) +
                     QLatin1Char('.') + ext));
    };
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, syncOutput);
    connect(fileTemplateEdit_, &QLineEdit::textChanged, this, syncOutput);
    syncOutput();

    autoSaveCheck_ = new QCheckBox(tr("Also save to the folder when copying"), this);
    autoSaveCheck_->setChecked(settings_.autoSave());
    form->addRow(QString(), autoSaveCheck_);

    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildTextPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // 测试行:按钮 + 结果(结果可折行,失败原因常较长)
    const auto testRow = [this](QPushButton*& button, QLabel*& result) {
        auto* row = new QWidget(this);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(10);
        button = new QPushButton(icons::lightIcon("arrow_sync"), tr("Test connection"), this);
        result = new QLabel(this);
        result->setWordWrap(true);
        result->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(button, 0, Qt::AlignTop);
        h->addWidget(result, 1);
        return row;
    };

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
    ocrForm->addRow(QString(), testRow(ocrTestBtn_, ocrTestResult_));
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
    connect(ocrTestBtn_, &QPushButton::clicked, this, [this] {
        ocrTestBtn_->setEnabled(false);
        showResultText(ocrTestResult_, ResultTone::Pending, tr("Testing..."));
        emit ocrTestRequested(ocrProtocolCombo_->currentData().toString(),
                              ocrUrlEdit_->text().trimmed(), ocrKeyEdit_->text().trimmed(),
                              ocrModelEdit_->text().trimmed());
    });
    layout->addWidget(ocrGroup);

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
    trForm->addRow(QString(), testRow(trTestBtn_, trTestResult_));
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
    connect(trTestBtn_, &QPushButton::clicked, this, [this] {
        trTestBtn_->setEnabled(false);
        showResultText(trTestResult_, ResultTone::Pending, tr("Testing..."));
        emit translateTestRequested(trProtocolCombo_->currentData().toString(),
                                    trUrlEdit_->text().trimmed(), trKeyEdit_->text().trimmed(),
                                    trAppIdEdit_->text().trimmed(),
                                    trModelEdit_->text().trimmed(),
                                    targetLangCombo_->currentData().toString());
    });
    layout->addWidget(trGroup);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildAboutPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // 品牌头:大 logo + 名称 + 版本 + 一句话介绍
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    auto* logo = new QLabel(page);
    QPixmap logoPm(QStringLiteral(":/icons/pixora-256.png"));
    logoPm.setDevicePixelRatio(logoPm.width() / 64.0);
    logo->setPixmap(logoPm);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* name = new QLabel(QStringLiteral("Pixora"), page);
    name->setStyleSheet(QStringLiteral("color:#1F2329; font-size:20px; font-weight:600;"));
    auto* version = new QLabel(tr("Version %1").arg(QApplication::applicationVersion()), page);
    auto* tagline = hintLabel(tr("Screenshot, annotate, pin and scrolling capture"), page);
    titles->addWidget(name);
    titles->addWidget(version);
    titles->addWidget(tagline);
    header->addWidget(logo, 0, Qt::AlignTop);
    header->addLayout(titles, 1);
    layout->addLayout(header);

    QFormLayout* form = nullptr;
    auto* card = makeCard(tr("Updates and support"), form);
    auto* updateRow = new QHBoxLayout;
    updateBtn_ = new QPushButton(icons::lightIcon("arrow_sync"), tr("Check for updates"), this);
    updateResult_ = new QLabel(this);
    updateResult_->setWordWrap(true);
    updateResult_->setOpenExternalLinks(true);
    updateRow->addWidget(updateBtn_);
    updateRow->addWidget(updateResult_, 1);
    form->addRow(updateRow);
    connect(updateBtn_, &QPushButton::clicked, this, [this] {
        updateBtn_->setEnabled(false);
        showResultText(updateResult_, ResultTone::Pending, tr("Checking..."));
        emit updateCheckRequested();
    });

    auto* linkRow = new QHBoxLayout;
    auto* homepageBtn = new QPushButton(icons::lightIcon("open"), tr("Project homepage"), this);
    connect(homepageBtn, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(kHomepage)); });
    auto* logsBtn =
        new QPushButton(icons::lightIcon("folder_open"), tr("Open log folder"), this);
    connect(logsBtn, &QPushButton::clicked, this, [] {
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
            QStringLiteral("/logs");
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    linkRow->addWidget(homepageBtn);
    linkRow->addWidget(logsBtn);
    linkRow->addStretch();
    form->addRow(linkRow);
    form->addRow(hintLabel(tr("When reporting a problem, please attach the files in the "
                              "log folder."),
                           this));
    layout->addWidget(card);

    layout->addWidget(hintLabel(tr("Open source under GPL-3.0. Icons: Fluent UI System "
                                   "Icons (MIT)."),
                                page));
    layout->addStretch();
    return page;
}

void SettingsDialog::markHotkeyConflicts(bool captureFailed, bool pinFailed,
                                        bool repeatFailed) {
    // 红框画在内部 QLineEdit 上(QKeySequenceEdit 本体不绘制边框)
    const QString style = QStringLiteral("QLineEdit { border: 1px solid %1; }")
                              .arg(theme::danger().name());
    captureEdit_->setStyleSheet(captureFailed ? style : QString());
    pinEdit_->setStyleSheet(pinFailed ? style : QString());
    repeatEdit_->setStyleSheet(repeatFailed ? style : QString());
    const bool anyFailed = captureFailed || pinFailed || repeatFailed;
    hotkeyWarning_->setVisible(anyFailed);
    if (anyFailed) {
        nav_->setCurrentRow(1); // 直接打开快捷键页
    }
}

void SettingsDialog::showOcrTestResult(bool ok, const QString& detail) {
    ocrTestBtn_->setEnabled(true);
    showResultText(ocrTestResult_, ok ? ResultTone::Ok : ResultTone::Error, detail);
}

void SettingsDialog::showTranslateTestResult(bool ok, const QString& detail) {
    trTestBtn_->setEnabled(true);
    showResultText(trTestResult_, ok ? ResultTone::Ok : ResultTone::Error, detail);
}

void SettingsDialog::showUpdateCheckResult(bool ok, bool newer, const QString& version,
                                           const QString& url) {
    updateBtn_->setEnabled(true);
    if (!ok) {
        showResultText(updateResult_, ResultTone::Error,
                  tr("Couldn't check for updates: %1").arg(version));
    } else if (newer) {
        showResultText(updateResult_, ResultTone::Ok,
                  tr("Pixora %1 is available. <a href=\"%2\">Open the download page</a>")
                      .arg(version.toHtmlEscaped(), url.toHtmlEscaped()));
    } else {
        showResultText(updateResult_, ResultTone::Ok, tr("You're up to date"));
    }
}

void SettingsDialog::apply() {
    // 空热键保持原值(避免误清空导致功能不可达)
    if (!captureEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyCaptureRegion(captureEdit_->keySequence());
    }
    if (!pinEdit_->keySequence().isEmpty()) {
        settings_.setHotkeyPinFromClipboard(pinEdit_->keySequence());
    }
    // 重做上次选区默认不绑定,允许清空 → 直接写入(含空序列即解绑)
    settings_.setHotkeyRepeatLastRegion(repeatEdit_->keySequence());
    settings_.setLanguage(languageCombo_->currentData().toString());
    settings_.setCheckUpdates(updateCheck_->isChecked());
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
    // 只在用户改动复选框时写注册表:每次保存都重写 Run 项,安全软件会反复弹
    // "修改启动项"拦截,用户一点拦截反倒把自启关掉
    if (system_ && autoStartCheck_->isChecked() != autoStartInitial_) {
        const bool want = autoStartCheck_->isChecked();
        const bool ok = system_->setAutoStart(want);
        const bool enabled = system_->isAutoStartEnabled();
        // 持久化"实际达成"的状态作为意图:成功→true(启动时对账维持);
        // 失败→存为真实(false),不每次启动反复重试打扰用户
        settings_.setAutoStartDesired(enabled);
        autoStartInitial_ = enabled;
        if (want && !ok) {
            // 写入被安全软件/权限挡下:复选框回到真实状态,并明确告知原因
            autoStartCheck_->setChecked(enabled);
            QMessageBox::warning(
                this, tr("Start at login"),
                tr("Couldn't turn on start at login. Security software may have "
                   "blocked it - allow Pixora to change startup items in your "
                   "antivirus, then try again."));
        }
    }
    emit applied();
}

} // namespace pixora
