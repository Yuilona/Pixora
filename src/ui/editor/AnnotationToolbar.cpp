#include "ui/editor/AnnotationToolbar.h"

#include "core/capture/SnipSession.h"
#include "ui/InstantTip.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace pixora {

namespace {

struct ToolSpec {
    AnnotationTool tool;
    const char* label;
};

// 标签经 QT_TRANSLATE_NOOP 标记,实际显示时再 tr()(上下文与类一致)
constexpr std::array<ToolSpec, 9> kTools = {{
    {AnnotationTool::Rect, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Rectangle")},
    {AnnotationTool::Ellipse, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Ellipse")},
    {AnnotationTool::Arrow, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Arrow")},
    {AnnotationTool::Pen, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Pen")},
    {AnnotationTool::Marker, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Marker")},
    {AnnotationTool::Text, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Text")},
    {AnnotationTool::Badge,
     QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Numbered badge")},
    {AnnotationTool::Mosaic, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Mosaic")},
    {AnnotationTool::Blur, QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "Blur")},
}};

const std::array<QColor, 6> kPalette = {
    QColor(0xE5, 0x39, 0x35), // 红
    QColor(0xFD, 0xD8, 0x35), // 黄
    QColor(0x43, 0xA0, 0x47), // 绿
    QColor(0x1E, 0x88, 0xE5), // 蓝
    QColor(Qt::white),
    QColor(Qt::black),
};

// 大小三档:线宽 2/4/8(文字字号、序号半径、马赛克强度均由线宽派生,
// 见 AnnotationTypes.h);圆点直径示意档位
constexpr std::array<int, 3> kWidths = {2, 4, 8};
constexpr std::array<qreal, 3> kSizeDots = {4, 7, 11};
constexpr const char* kWidthNames[] = {
    QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "thin"),
    QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "medium"),
    QT_TRANSLATE_NOOP("pixora::AnnotationToolbar", "thick"),
};

constexpr int kRowSpacing = 6; // 主栏与情境栏卡片间距

bool hasColor(AnnotationTool tool) {
    return tool != AnnotationTool::Mosaic && tool != AnnotationTool::Blur;
}

// 情境栏圆点按钮(颜色色点 / 大小档位点):选中 = 白色外环,悬停 = 淡环。
// 点自带淡描边,黑色色点落在深色卡片上也看得见。
class DotButton : public QAbstractButton {
public:
    DotButton(const QColor& fill, qreal diameter, qreal ringRadius, QWidget* parent)
        : QAbstractButton(parent), fill_(fill), diameter_(diameter),
          ringRadius_(ringRadius) {
        setCheckable(true);
        setFixedSize(28, 28);
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c = QRectF(rect()).center();
        if (isChecked() || underMouse()) {
            QColor ring = Qt::white;
            if (!isChecked()) {
                ring.setAlpha(80);
            }
            p.setPen(QPen(ring, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, ringRadius_, ringRadius_);
        }
        p.setPen(QPen(QColor(255, 255, 255, 70), 1));
        p.setBrush(fill_);
        p.drawEllipse(c, diameter_ / 2, diameter_ / 2);
    }

private:
    QColor fill_;
    qreal diameter_;
    qreal ringRadius_;
};

// 卡片内一行:左右留卡片内边距
QHBoxLayout* rowLayout(QWidget* row) {
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);
    return layout;
}

void addSeparator(QWidget* parent, QHBoxLayout* layout) {
    auto* sep = new QWidget(parent);
    sep->setFixedSize(1, 18);
    const QColor line = theme::hairline();
    sep->setStyleSheet(QStringLiteral("background: rgba(%1,%2,%3,%4);")
                           .arg(line.red())
                           .arg(line.green())
                           .arg(line.blue())
                           .arg(line.alpha()));
    layout->addSpacing(3);
    layout->addWidget(sep);
    layout->addSpacing(3);
}

} // namespace

AnnotationToolbar::AnnotationToolbar(SnipSession& session) : session_(session) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground); // 配合 paintEvent 自绘卡片 + 投影
    setStyleSheet(theme::chromeStyleSheet(theme::fontsize::label, /*padV=*/6, /*padH=*/7));

    rows_ = new QVBoxLayout(this);
    constexpr int m = theme::kShadowMargin; // 四周留出投影边距
    rows_->setContentsMargins(m, m, m, m);
    rows_->setSpacing(kRowSpacing);
    rows_->setSizeConstraint(QLayout::SetFixedSize);

    mainRow_ = buildMainRow();
    contextRow_ = buildContextRow();
    rows_->addWidget(mainRow_, 0, Qt::AlignRight);
    rows_->addWidget(contextRow_, 0, Qt::AlignLeft);
    contextRow_->hide();

    connect(&session_, &SnipSession::interactionFinished, this, [this] {
        reposition();
        show();
    });
    connect(&session_, &SnipSession::selectionChanged, this, [this] {
        if (isVisible()) {
            reposition();
        }
    });
    connect(&session_, &SnipSession::activeToolChanged, this,
            &AnnotationToolbar::syncFromSession);
    connect(&session_, &SnipSession::annotationsChanged, this,
            &AnnotationToolbar::syncFromSession);
    syncFromSession();
}

QWidget* AnnotationToolbar::buildMainRow() {
    auto* row = new QWidget(this);
    auto* layout = rowLayout(row);
    auto* tip = new InstantTip(this);

    auto addIconButton = [this, row, layout, tip](const QIcon& icon, const QString& tooltip,
                                                  auto onClicked) {
        auto* btn = new QToolButton(row);
        btn->setIcon(icon);
        btn->setIconSize(QSize(20, 20));
        btn->setToolTip(tooltip);
        btn->installEventFilter(tip);
        connect(btn, &QToolButton::clicked, this, onClicked);
        layout->addWidget(btn);
        return btn;
    };

    for (const ToolSpec& spec : kTools) {
        QToolButton* btn = addIconButton(icons::toolIcon(spec.tool), tr(spec.label),
                                         [this, spec](bool checked) {
                                             session_.setActiveTool(
                                                 checked ? std::optional(spec.tool)
                                                         : std::nullopt);
                                         });
        btn->setCheckable(true);
        toolButtons_.push_back(btn);
    }

    addSeparator(row, layout);

    QUndoStack& undoStack = session_.document().undoStack();
    QToolButton* undoBtn = addIconButton(icons::undoIcon(), tr("Undo (Ctrl+Z)"),
                                         [&undoStack] { undoStack.undo(); });
    QToolButton* redoBtn = addIconButton(icons::redoIcon(), tr("Redo (Ctrl+Y)"),
                                         [&undoStack] { undoStack.redo(); });
    undoBtn->setEnabled(undoStack.canUndo());
    redoBtn->setEnabled(undoStack.canRedo());
    connect(&undoStack, &QUndoStack::canUndoChanged, undoBtn, &QToolButton::setEnabled);
    connect(&undoStack, &QUndoStack::canRedoChanged, redoBtn, &QToolButton::setEnabled);

    addSeparator(row, layout);

    addIconButton(icons::ocrIcon(), tr("Extract text"),
                  [this] { session_.requestExtractText(); });
    addIconButton(icons::translateIcon(), tr("Translate"),
                  [this] { session_.requestTranslate(); });
    addIconButton(icons::scrollIcon(), tr("Scrolling capture"),
                  [this] { session_.requestScroll(); });

    addSeparator(row, layout);

    addIconButton(icons::pinIcon(), tr("Pin"), [this] { session_.requestPin(); });
    addIconButton(icons::saveIcon(), tr("Save as (Ctrl+S)"),
                  [this] { session_.requestSave(); });
    addIconButton(icons::cancelIcon(), tr("Cancel"), [this] { session_.cancel(); });

    // 最高频出口:图标 + 文字的实心主按钮,与其余图标按钮拉开层级
    layout->addSpacing(4);
    QToolButton* copyBtn = addIconButton(icons::primaryConfirmIcon(),
                                         tr("Copy and finish (Enter)"),
                                         [this] { session_.confirm(); });
    copyBtn->setObjectName(QStringLiteral("primary"));
    copyBtn->setText(tr("Copy"));
    copyBtn->setIconSize(QSize(18, 18));
    copyBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    return row;
}

QWidget* AnnotationToolbar::buildContextRow() {
    auto* row = new QWidget(this);
    auto* layout = rowLayout(row);
    auto* tip = new InstantTip(this);

    // 颜色组(马赛克/模糊无颜色,整组连同分隔线隐藏)
    colorGroup_ = new QWidget(row);
    auto* colorLayout = new QHBoxLayout(colorGroup_);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(0);
    for (const QColor& color : kPalette) {
        auto* btn = new DotButton(color, /*diameter=*/16, /*ringRadius=*/12, colorGroup_);
        connect(btn, &QAbstractButton::clicked, this, [this, color] {
            session_.chooseColor(color); // 有选中条目时同时改它(可撤销)
            syncFromSession();
        });
        colorLayout->addWidget(btn);
        colorButtons_.push_back(btn);
    }
    addSeparator(colorGroup_, colorLayout);
    layout->addWidget(colorGroup_);

    for (size_t i = 0; i < kWidths.size(); ++i) {
        auto* btn = new DotButton(theme::hudIcon(), kSizeDots[i], /*ringRadius=*/10, row);
        btn->setToolTip(tr("Size: %1").arg(tr(kWidthNames[i])));
        btn->installEventFilter(tip);
        const int width = kWidths[i];
        connect(btn, &QAbstractButton::clicked, this, [this, width] {
            session_.chooseWidth(width);
            syncFromSession();
        });
        layout->addWidget(btn);
        sizeButtons_.push_back(btn);
    }
    return row;
}

void AnnotationToolbar::syncFromSession() {
    const std::optional<AnnotationTool> tool = session_.activeTool();
    for (size_t i = 0; i < toolButtons_.size(); ++i) {
        toolButtons_[i]->setChecked(tool == kTools[i].tool);
    }

    // 情境栏作用对象:激活的工具(新画条目的默认样式),否则选中的已有条目
    const AnnotationItem* item = tool ? nullptr : session_.selectedItem();
    const bool showContext = tool.has_value() || item;
    if (showContext) {
        const AnnotationTool kind = tool ? *tool : item->tool();
        const StrokeStyle style = item ? item->style() : session_.strokeStyle();
        colorGroup_->setVisible(hasColor(kind));
        for (size_t i = 0; i < colorButtons_.size(); ++i) {
            colorButtons_[i]->setChecked(kPalette[i] == style.color);
        }
        for (size_t i = 0; i < sizeButtons_.size(); ++i) {
            sizeButtons_[i]->setChecked(kWidths[i] == style.width);
        }
    }
    if (contextRow_->isVisible() != showContext) {
        contextRow_->setVisible(showContext);
        if (isVisible()) {
            reposition(); // 高度变了;贴在选区上方时须保持主栏不动
        }
    }
    update();
}

void AnnotationToolbar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    for (QWidget* row : {mainRow_, contextRow_}) {
        if (row->isVisible()) {
            theme::paintShadow(p, row->geometry(), theme::corner::card);
            theme::paintCard(p, row->geometry(), theme::corner::card);
        }
    }
}

void AnnotationToolbar::reposition() {
    const QRect sel = session_.selection();
    const QRect bounds = session_.snapshot().virtualGeometryLogical();
    constexpr int m = theme::kShadowMargin;

    // 先按"主栏在上、情境栏在下"量出卡片区尺寸
    rows_->removeWidget(contextRow_);
    rows_->insertWidget(1, contextRow_, 0, Qt::AlignLeft);
    adjustSize();
    const int cardW = width() - 2 * m;
    const int cardH = height() - 2 * m;

    // 按卡片(窗口内缩投影边距)对齐选区,投影可伸出屏幕边缘
    QPoint pos(sel.right() - cardW + 1, sel.bottom() + 8);
    bool above = false;
    if (pos.y() + cardH > bounds.bottom()) {
        pos.setY(sel.top() - cardH - 8); // 下方放不下 → 选区上方
        above = true;
    }
    if (pos.y() < bounds.top()) {
        pos.setY(sel.bottom() - cardH - 8); // 还不行 → 选区内部底边
        above = true;
    }
    if (above) {
        // 贴在上方时情境栏翻到主栏之上,主栏始终紧挨选区
        rows_->removeWidget(contextRow_);
        rows_->insertWidget(0, contextRow_, 0, Qt::AlignLeft);
    }
    pos.setX(std::clamp(pos.x(), bounds.left(), bounds.right() - cardW + 1));
    move(pos - QPoint(m, m));
}

} // namespace pixora
