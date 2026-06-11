#include "ui/editor/AnnotationToolbar.h"

#include "core/capture/SnipSession.h"

#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QToolButton>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <functional>

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

// —— 程序绘制的工具图标(16x16 逻辑,2x 渲染保证高 DPI 清晰)——
// 不引入图片资源;线条色与按钮文字色一致,选中态蓝底上同样可读。

const QColor kIconColor(0xDD, 0xDD, 0xDD);

QIcon makeIcon(const std::function<void(QPainter&)>& draw) {
    QPixmap pm(32, 32);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(kIconColor, 1.6));
    p.setBrush(Qt::NoBrush);
    draw(p);
    return QIcon(pm);
}

QIcon toolIcon(AnnotationTool tool) {
    switch (tool) {
    case AnnotationTool::Rect:
        return makeIcon([](QPainter& p) { p.drawRect(QRectF(2.5, 3.5, 11, 9)); });
    case AnnotationTool::Ellipse:
        return makeIcon([](QPainter& p) { p.drawEllipse(QRectF(2.5, 3, 11, 10)); });
    case AnnotationTool::Arrow:
        return makeIcon([](QPainter& p) {
            p.drawLine(QPointF(3.5, 12.5), QPointF(12, 4));
            QPainterPath head(QPointF(12.5, 3.5));
            head.lineTo(QPointF(8.5, 4.5));
            head.lineTo(QPointF(11.5, 7.5));
            head.closeSubpath();
            p.fillPath(head, kIconColor);
        });
    case AnnotationTool::Pen:
        return makeIcon([](QPainter& p) {
            p.drawLine(QPointF(5, 11), QPointF(12, 4));
            QPainterPath nib(QPointF(4.2, 11.8));
            nib.lineTo(QPointF(3, 13));
            nib.lineTo(QPointF(4.8, 12.6));
            nib.closeSubpath();
            p.fillPath(nib, kIconColor);
        });
    case AnnotationTool::Marker:
        return makeIcon([](QPainter& p) {
            QColor c = kIconColor;
            c.setAlpha(150);
            p.setPen(QPen(c, 5, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(4, 12), QPointF(12, 4));
        });
    case AnnotationTool::Text:
        return makeIcon([](QPainter& p) {
            QFont f = p.font();
            f.setPixelSize(12);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 16, 16), Qt::AlignCenter, QStringLiteral("T"));
        });
    case AnnotationTool::Badge:
        return makeIcon([](QPainter& p) {
            p.drawEllipse(QRectF(2.5, 2.5, 11, 11));
            QFont f = p.font();
            f.setPixelSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(2.5, 2.5, 11, 11), Qt::AlignCenter, QStringLiteral("1"));
        });
    case AnnotationTool::Mosaic:
        return makeIcon([](QPainter& p) {
            p.setPen(Qt::NoPen);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    QColor c = kIconColor;
                    c.setAlpha((row + col) % 2 ? 90 : 220);
                    p.fillRect(QRectF(2.5 + col * 4, 2.5 + row * 4, 3.6, 3.6), c);
                }
            }
        });
    case AnnotationTool::Blur:
        return makeIcon([](QPainter& p) {
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 3; ++i) {
                QColor c = kIconColor;
                c.setAlpha(220 - i * 75);
                const qreal r = 2.0 + i * 2.2;
                p.setBrush(c);
                p.drawEllipse(QPointF(8, 8), r, r);
            }
        });
    }
    return {};
}

// 粗细按钮:横线粗细即当前档位
QIcon widthIcon(int width) {
    return makeIcon([width](QPainter& p) {
        p.setPen(QPen(kIconColor, std::clamp(width * 0.55, 1.2, 4.5), Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(QPointF(3, 8), QPointF(13, 8));
    });
}

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
        auto* btn = new QToolButton(this);
        btn->setIcon(toolIcon(spec.tool));
        btn->setIconSize(QSize(16, 16));
        btn->setToolTip(QString::fromUtf8(spec.label));
        btn->setCheckable(true);
        btn->installEventFilter(this); // 自管理悬浮提示
        connect(btn, &QToolButton::clicked, this,
                [this, spec](bool checked) { chooseTool(spec.tool, checked); });
        layout->addWidget(btn);
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
            session_.chooseColor(color); // 有选中条目时同时改它(可撤销)
        });
        layout->addWidget(btn);
        colorButtons_.push_back(btn);
    }

    widthButton_ = new QToolButton(this);
    widthButton_->setIcon(widthIcon(kWidths[static_cast<size_t>(widthIndex_)]));
    widthButton_->setIconSize(QSize(16, 16));
    widthButton_->installEventFilter(this);
    widthButton_->setToolTip(
        QStringLiteral("线条粗细:%1(点击切换)").arg(QString::fromUtf8(kWidthNames[widthIndex_])));
    connect(widthButton_, &QToolButton::clicked, this, [this] {
        widthIndex_ = (widthIndex_ + 1) % static_cast<int>(kWidths.size());
        const int width = kWidths[static_cast<size_t>(widthIndex_)];
        widthButton_->setIcon(widthIcon(width));
        widthButton_->setToolTip(QStringLiteral("线条粗细:%1(点击切换)")
                                     .arg(QString::fromUtf8(kWidthNames[widthIndex_])));
        session_.chooseWidth(width);
    });
    layout->addWidget(widthButton_);

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
}

bool AnnotationToolbar::eventFilter(QObject* watched, QEvent* event) {
    auto* btn = qobject_cast<QToolButton*>(watched);
    if (!btn || btn->toolTip().isEmpty()) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::Enter:
        // 即时显示,不做延迟
        QToolTip::showText(QCursor::pos() + QPoint(0, 14), btn->toolTip(), btn);
        break;
    case QEvent::Leave:
    case QEvent::MouseButtonPress:
        QToolTip::hideText();
        break;
    case QEvent::ToolTip:
        return true; // 接管默认 tooltip 通道,避免双重显示
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
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
