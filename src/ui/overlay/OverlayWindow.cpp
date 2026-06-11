#include "ui/overlay/OverlayWindow.h"

#include "core/capture/SnipSession.h"
#include "ui/editor/AnnotationRenderer.h"
#include "ui/overlay/Magnifier.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace pixora {

namespace {
const QColor kMaskColor(0, 0, 0, 120);
const QColor kBorderColor(45, 124, 246); // Snipaste 风格蓝
constexpr int kDragThreshold = 4;        // 区分"点击吸附"与"拖拽选区"
} // namespace

OverlayWindow::OverlayWindow(const ScreenSnap& snap, SnipSession& session)
    : session_(session), frozen_(snap.image), physical_(snap.image), dpr_(snap.dpr) {
    frozen_.setDevicePixelRatio(snap.dpr);

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(snap.geometryLogical);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    connect(&session_, &SnipSession::selectionChanged, this,
            qOverload<>(&QWidget::update));
    connect(&session_, &SnipSession::hoverChanged, this, qOverload<>(&QWidget::update));
    connect(&session_, &SnipSession::annotationsChanged, this,
            qOverload<>(&QWidget::update));
    connect(&session_, &SnipSession::activeToolChanged, this, [this] {
        setCursor(session_.activeTool() ? Qt::CrossCursor : Qt::ArrowCursor);
    });
}

QRect OverlayWindow::toLocal(const QRect& globalRect) const {
    return globalRect.translated(-geometry().topLeft()) & rect();
}

QRect OverlayWindow::selectionLocal() const {
    return session_.hasSelection() ? toLocal(session_.selection()) : QRect();
}

bool OverlayWindow::magnifierVisible() const {
    if (!hasCursor_) {
        return false;
    }
    return !session_.hasSelection() || mode_ == Mode::Creating || mode_ == Mode::Resizing;
}

void OverlayWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.drawImage(0, 0, frozen_);

    // 高亮区:已有选区优先,否则为悬停吸附的窗口
    const bool hasSelection = session_.hasSelection();
    const QRect active = hasSelection ? selectionLocal() : toLocal(session_.hoverRect());

    QRegion dimmed(rect());
    if (!active.isEmpty()) {
        dimmed -= active;
    }
    painter.setClipRegion(dimmed);
    painter.fillRect(rect(), kMaskColor);
    painter.setClipping(false);

    // 标注内容(裁剪到选区;条目为全局逻辑坐标 → 平移到本窗局部)
    if (hasSelection && !active.isEmpty()) {
        painter.save();
        painter.setClipRect(active);
        painter.translate(-geometry().topLeft());
        AnnotationRenderer::render(painter, session_.document());
        if (const AnnotationItem* pending = session_.pendingAnnotation()) {
            AnnotationRenderer::renderItem(painter, *pending);
        }
        painter.restore();
    }

    if (!active.isEmpty()) {
        painter.setPen(QPen(kBorderColor, 2));
        painter.drawRect(QRectF(active).adjusted(0.5, 0.5, -0.5, -0.5));

        if (hasSelection && mode_ != Mode::Creating && !session_.activeTool()) {
            SelectionHandles::paint(painter, active, kBorderColor);
        }

        // 尺寸标签(逻辑像素)
        const QRect global = hasSelection ? session_.selection() : session_.hoverRect();
        const QString label =
            QStringLiteral("%1 × %2").arg(global.width()).arg(global.height());
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        const QRect textRect =
            painter.fontMetrics().boundingRect(label).adjusted(-6, -3, 6, 3);
        QPoint anchor(active.left(), active.top() - textRect.height() - 4);
        if (anchor.y() < 0) {
            anchor.setY(active.top() + 4);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.drawRoundedRect(QRect(anchor, textRect.size()), 3, 3);
        painter.setPen(Qt::white);
        painter.drawText(QRect(anchor, textRect.size()), Qt::AlignCenter, label);
    }

    if (magnifierVisible()) {
        Magnifier::Context ctx;
        ctx.physicalImage = &physical_;
        ctx.dpr = dpr_;
        ctx.cursorLocalLogical = cursorLocal_;
        ctx.cursorGlobalLogical = cursorLocal_ + geometry().topLeft();
        ctx.widgetSize = size();
        Magnifier::draw(painter, ctx);
    }
}

void OverlayWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    pressGlobal_ = event->globalPosition().toPoint();
    moved_ = false;

    // 标注工具激活时,选区内按下 = 开始绘制
    if (session_.hasSelection() && session_.activeTool() &&
        session_.selection().contains(pressGlobal_)) {
        mode_ = Mode::Drawing;
        session_.beginAnnotation(pressGlobal_);
        return;
    }

    if (session_.hasSelection()) {
        const SelectionHandles::Hit hit =
            SelectionHandles::hitTest(selectionLocal(), event->pos());
        if (hit == SelectionHandles::Hit::Inside) {
            mode_ = Mode::Moving;
            baseSelection_ = session_.selection();
            grabOffset_ = pressGlobal_ - baseSelection_.topLeft();
            return;
        }
        if (hit != SelectionHandles::Hit::None) {
            mode_ = Mode::Resizing;
            activeHandle_ = hit;
            baseSelection_ = session_.selection();
            return;
        }
    }
    mode_ = Mode::Creating;
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    cursorLocal_ = event->pos();
    hasCursor_ = true;
    const QPoint global = event->globalPosition().toPoint();

    switch (mode_) {
    case Mode::Creating:
        if (!moved_ && (global - pressGlobal_).manhattanLength() > kDragThreshold) {
            moved_ = true;
        }
        if (moved_) {
            session_.setSelection(QRect(pressGlobal_, global));
        }
        break;
    case Mode::Moving: {
        moved_ = true;
        const QRect bounds = session_.snapshot().virtualGeometryLogical();
        QPoint topLeft = global - grabOffset_;
        topLeft.setX(std::clamp(topLeft.x(), bounds.left(),
                                bounds.right() - baseSelection_.width() + 1));
        topLeft.setY(std::clamp(topLeft.y(), bounds.top(),
                                bounds.bottom() - baseSelection_.height() + 1));
        session_.setSelection(QRect(topLeft, baseSelection_.size()));
        break;
    }
    case Mode::Resizing:
        moved_ = true;
        session_.setSelection(
            SelectionHandles::resized(baseSelection_, activeHandle_, global - pressGlobal_));
        break;
    case Mode::Drawing:
        session_.updateAnnotation(global);
        break;
    case Mode::Idle:
        if (session_.hasSelection()) {
            if (!session_.activeTool()) {
                setCursor(SelectionHandles::cursorFor(
                    SelectionHandles::hitTest(selectionLocal(), event->pos())));
            }
        } else {
            session_.updateHover(global);
        }
        break;
    }
    update();
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (mode_ == Mode::Drawing) {
        session_.endAnnotation();
        mode_ = Mode::Idle;
        return;
    }
    // 单击(未拖出阈值)且悬停命中窗口 → 吸附为选区
    if (mode_ == Mode::Creating && !moved_ && !session_.hasSelection() &&
        !session_.hoverRect().isEmpty()) {
        session_.setSelection(session_.hoverRect());
    }
    mode_ = Mode::Idle;
    session_.notifyInteractionFinished();
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton &&
        session_.selection().contains(event->globalPosition().toPoint())) {
        session_.confirm();
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        // 有激活的标注工具时先退出工具,再按才取消会话
        if (session_.activeTool()) {
            session_.setActiveTool(std::nullopt);
        } else {
            session_.cancel();
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        session_.confirm();
        break;
    case Qt::Key_F3:
        session_.requestPin();
        break;
    case Qt::Key_S:
        if (event->modifiers() & Qt::ControlModifier) {
            session_.requestSave();
        }
        break;
    case Qt::Key_Z:
        if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
            session_.document().undoStack().redo();
        } else if (event->modifiers() & Qt::ControlModifier) {
            session_.document().undoStack().undo();
        }
        break;
    case Qt::Key_Y:
        if (event->modifiers() & Qt::ControlModifier) {
            session_.document().undoStack().redo();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace pixora
