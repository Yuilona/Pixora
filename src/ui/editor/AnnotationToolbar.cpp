#include "ui/editor/AnnotationToolbar.h"

#include "core/capture/SnipSession.h"
#include "ui/InstantTip.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QScreen>
#include <QToolButton>

#include <algorithm>
#include <array>

namespace pixora {

namespace {

struct ToolSpec {
    AnnotationTool tool;
    const char* label;
};

constexpr std::array<ToolSpec, 9> kTools = {{
    {AnnotationTool::Rect, "矩形"},
    {AnnotationTool::Ellipse, "椭圆"},
    {AnnotationTool::Arrow, "箭头"},
    {AnnotationTool::Pen, "画笔"},
    {AnnotationTool::Marker, "马克笔"},
    {AnnotationTool::Text, "文字"},
    {AnnotationTool::Badge, "序号"},
    {AnnotationTool::Mosaic, "马赛克"},
    {AnnotationTool::Blur, "模糊"},
}};

const std::array<QColor, 6> kPalette = {
    QColor(0xE5, 0x39, 0x35), // 红
    QColor(0xFD, 0xD8, 0x35), // 黄
    QColor(0x43, 0xA0, 0x47), // 绿
    QColor(0x1E, 0x88, 0xE5), // 蓝
    QColor(Qt::white),
    QColor(Qt::black),
};

constexpr std::array<int, 3> kWidths = {2, 4, 8};
constexpr const char* kWidthNames[] = {"细", "中", "粗"};

} // namespace

AnnotationToolbar::AnnotationToolbar(SnipSession& session) : session_(session) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground); // 配合 paintCard 圆角卡片底
    setStyleSheet(theme::chromeStyleSheet(/*fontPx=*/13, /*padV=*/6, /*padH=*/10));

    auto* tip = new InstantTip(this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    // 分组细分隔线:全图标后靠分组保持可扫读性
    auto addSeparator = [this, layout] {
        auto* sep = new QWidget(this);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QStringLiteral("background: rgba(255,255,255,24);"));
        layout->addSpacing(3);
        layout->addWidget(sep);
        layout->addSpacing(3);
    };

    auto addIconButton = [this, layout, tip](const QIcon& icon, const QString& tooltip,
                                             auto onClicked) {
        auto* btn = new QToolButton(this);
        btn->setIcon(icon);
        btn->setIconSize(QSize(20, 20));
        btn->setToolTip(tooltip);
        btn->installEventFilter(tip);
        connect(btn, &QToolButton::clicked, this, onClicked);
        layout->addWidget(btn);
        return btn;
    };

    for (const ToolSpec& spec : kTools) {
        auto* btn = new QToolButton(this);
        btn->setIcon(icons::toolIcon(spec.tool));
        btn->setIconSize(QSize(20, 20));
        btn->setToolTip(QString::fromUtf8(spec.label));
        btn->setCheckable(true);
        btn->installEventFilter(tip);
        connect(btn, &QToolButton::clicked, this,
                [this, spec](bool checked) { chooseTool(spec.tool, checked); });
        layout->addWidget(btn);
        toolButtons_.push_back(btn);
    }

    addSeparator();

    for (const QColor& color : kPalette) {
        auto* btn = new QToolButton(this);
        btn->setFixedSize(24, 24);
        btn->setCheckable(true);
        btn->setChecked(color == session_.strokeStyle().color);
        btn->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 1px "
                                          "solid #555; } QToolButton:checked { border: "
                                          "2px solid white; }")
                               .arg(color.name()));
        connect(btn, &QToolButton::clicked, this, [this, btn, color] {
            for (QToolButton* other : colorButtons_) {
                other->setChecked(other == btn);
            }
            session_.chooseColor(color); // 有选中条目时同时改它(可撤销)
        });
        layout->addWidget(btn);
        colorButtons_.push_back(btn);
    }

    widthButton_ = new QToolButton(this);
    widthButton_->setIcon(icons::widthIcon(kWidths[static_cast<size_t>(widthIndex_)]));
    widthButton_->setIconSize(QSize(20, 20));
    widthButton_->installEventFilter(tip);
    widthButton_->setToolTip(
        QStringLiteral("线条粗细:%1(点击切换)").arg(QString::fromUtf8(kWidthNames[widthIndex_])));
    connect(widthButton_, &QToolButton::clicked, this, [this] {
        widthIndex_ = (widthIndex_ + 1) % static_cast<int>(kWidths.size());
        const int width = kWidths[static_cast<size_t>(widthIndex_)];
        widthButton_->setIcon(icons::widthIcon(width));
        widthButton_->setToolTip(QStringLiteral("线条粗细:%1(点击切换)")
                                     .arg(QString::fromUtf8(kWidthNames[widthIndex_])));
        session_.chooseWidth(width);
    });
    layout->addWidget(widthButton_);

    addSeparator();

    QToolButton* undoBtn =
        addIconButton(icons::undoIcon(), QStringLiteral("撤销 (Ctrl+Z)"),
                      [this] { session_.document().undoStack().undo(); });
    QToolButton* redoBtn =
        addIconButton(icons::redoIcon(), QStringLiteral("重做 (Ctrl+Y)"),
                      [this] { session_.document().undoStack().redo(); });
    undoBtn->setEnabled(false);
    redoBtn->setEnabled(false);
    connect(&session_.document().undoStack(), &QUndoStack::canUndoChanged, undoBtn,
            &QToolButton::setEnabled);
    connect(&session_.document().undoStack(), &QUndoStack::canRedoChanged, redoBtn,
            &QToolButton::setEnabled);

    addSeparator();

    addIconButton(icons::ocrIcon(), QStringLiteral("提取文字"),
                  [this] { session_.requestExtractText(); });
    addIconButton(icons::translateIcon(), QStringLiteral("翻译"),
                  [this] { session_.requestTranslate(); });
    addIconButton(icons::scrollIcon(), QStringLiteral("长截图"),
                  [this] { session_.requestScroll(); });

    addSeparator();

    addIconButton(icons::pinIcon(), QStringLiteral("贴图"),
                  [this] { session_.requestPin(); });
    addIconButton(icons::saveIcon(), QStringLiteral("另存 (Ctrl+S)"),
                  [this] { session_.requestSave(); });
    addIconButton(icons::cancelIcon(), QStringLiteral("取消"),
                  [this] { session_.cancel(); });
    addIconButton(icons::confirmIcon(), QStringLiteral("复制并完成 (Enter)"),
                  [this] { session_.confirm(); });

    connect(&session_, &SnipSession::interactionFinished, this, [this] {
        reposition();
        show();
    });
    connect(&session_, &SnipSession::selectionChanged, this, [this] {
        if (isVisible()) {
            reposition();
        }
    });
}

void AnnotationToolbar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    theme::paintCard(p, rect(), 8);
}

void AnnotationToolbar::chooseTool(AnnotationTool tool, bool checked) {
    if (!checked) {
        session_.setActiveTool(std::nullopt);
        return;
    }
    for (size_t i = 0; i < toolButtons_.size(); ++i) {
        toolButtons_[i]->setChecked(kTools[i].tool == tool);
    }
    session_.setActiveTool(tool);
}

void AnnotationToolbar::reposition() {
    adjustSize();
    const QRect sel = session_.selection();
    const QRect bounds = session_.snapshot().virtualGeometryLogical();

    QPoint pos(sel.right() - width() + 1, sel.bottom() + 8);
    if (pos.y() + height() > bounds.bottom()) {
        pos.setY(sel.top() - height() - 8); // 下方放不下 → 选区上方
    }
    if (pos.y() < bounds.top()) {
        pos.setY(sel.bottom() - height() - 8); // 还不行 → 选区内部底边
    }
    pos.setX(std::clamp(pos.x(), bounds.left(), bounds.right() - width() + 1));
    move(pos);
}

} // namespace pixora
