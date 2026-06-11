#include "ui/editor/AnnotationToolbar.h"

#include "core/capture/SnipSession.h"

#include <QGuiApplication>
#include <QHBoxLayout>
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
    setStyleSheet(QStringLiteral(
        "QWidget { background: #2B2B2B; }"
        "QToolButton { color: #DDD; background: transparent; border: none;"
        "  padding: 4px 8px; font-size: 12px; }"
        "QToolButton:hover { background: #3D3D3D; }"
        "QToolButton:checked { background: #1E88E5; color: white; }"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    auto addButton = [this, layout](const QString& text, auto onClicked,
                                    bool checkable = false) {
        auto* btn = new QToolButton(this);
        btn->setText(text);
        btn->setCheckable(checkable);
        connect(btn, &QToolButton::clicked, this, onClicked);
        layout->addWidget(btn);
        return btn;
    };

    for (const ToolSpec& spec : kTools) {
        QToolButton* btn = addButton(
            QString::fromUtf8(spec.label),
            [this, spec](bool checked) { chooseTool(spec.tool, checked); },
            /*checkable=*/true);
        toolButtons_.push_back(btn);
    }

    for (const QColor& color : kPalette) {
        auto* btn = new QToolButton(this);
        btn->setFixedSize(20, 20);
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
            StrokeStyle style = session_.strokeStyle();
            style.color = color;
            session_.setStrokeStyle(style);
        });
        layout->addWidget(btn);
        colorButtons_.push_back(btn);
    }

    widthButton_ = addButton(QString::fromUtf8(kWidthNames[widthIndex_]), [this] {
        widthIndex_ = (widthIndex_ + 1) % static_cast<int>(kWidths.size());
        widthButton_->setText(QString::fromUtf8(kWidthNames[widthIndex_]));
        StrokeStyle style = session_.strokeStyle();
        style.width = kWidths[static_cast<size_t>(widthIndex_)];
        session_.setStrokeStyle(style);
    });

    QToolButton* undoBtn =
        addButton(QStringLiteral("撤销"), [this] { session_.document().undoStack().undo(); });
    QToolButton* redoBtn =
        addButton(QStringLiteral("重做"), [this] { session_.document().undoStack().redo(); });
    undoBtn->setEnabled(false);
    redoBtn->setEnabled(false);
    connect(&session_.document().undoStack(), &QUndoStack::canUndoChanged, undoBtn,
            &QToolButton::setEnabled);
    connect(&session_.document().undoStack(), &QUndoStack::canRedoChanged, redoBtn,
            &QToolButton::setEnabled);

    addButton(QStringLiteral("长截图"), [this] { session_.requestScroll(); });
    addButton(QStringLiteral("贴图"), [this] { session_.requestPin(); });
    addButton(QStringLiteral("另存"), [this] { session_.requestSave(); });
    addButton(QStringLiteral("复制"), [this] { session_.confirm(); });
    addButton(QStringLiteral("取消"), [this] { session_.cancel(); });

    connect(&session_, &SnipSession::interactionFinished, this, [this] {
        reposition();
        show();
    });
    connect(&session_, &SnipSession::selectionChanged, this, [this] {
        if (isVisible()) {
            reposition();
        }
    });
    // 悬停高亮窗口时直接亮出工具条,免去"先点一下确认"——
    // 点任意按钮时会话会把悬停窗口提升为正式选区
    connect(&session_, &SnipSession::hoverChanged, this, [this](const QRect& rect) {
        if (session_.hasSelection()) {
            return;
        }
        if (rect.isEmpty()) {
            hide();
            return;
        }
        reposition();
        show();
    });
    // 按下开始框选/吸附:先藏起来,拖拽过程不跟随乱跳,松手后再现
    connect(&session_, &SnipSession::interactionStarted, this, &QWidget::hide);
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
    const bool locked = session_.hasSelection();
    const QRect sel = locked ? session_.selection() : session_.hoverRect();
    const QRect bounds = session_.snapshot().virtualGeometryLogical();

    QPoint pos;
    if (locked) {
        pos = QPoint(sel.right() - width() + 1, sel.bottom() + 8);
        if (pos.y() + height() > bounds.bottom()) {
            pos.setY(sel.top() - height() - 8); // 下方放不下 → 选区上方
        }
        if (pos.y() < bounds.top()) {
            pos.setY(sel.bottom() - height() - 8); // 还不行 → 选区内部底边
        }
    } else {
        // 悬停预览:放在窗口内部右下角——光标移过去点按钮的路径
        // 始终在该窗口内,悬停目标不会中途切换导致工具条被"追着跑"
        pos = QPoint(sel.right() - width() + 1, sel.bottom() - height() - 8);
        pos.setY(std::max(pos.y(), sel.top()));
    }
    pos.setX(std::clamp(pos.x(), bounds.left(), bounds.right() - width() + 1));
    pos.setY(std::clamp(pos.y(), bounds.top(), bounds.bottom() - height() + 1));
    move(pos);
}

} // namespace pixora
