#include "ui/overlay/OverlayWindow.h"

#include "core/capture/SnipSession.h"
#include "platform/interface/ElementLocator.h"
#include "ui/editor/AnnotationRenderer.h"
#include "ui/overlay/Magnifier.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <memory>

namespace pixora {

namespace {
const QColor kMaskColor(0, 0, 0, 120);
const QColor kBorderColor(45, 124, 246); // Snipaste 风格蓝
constexpr int kDragThreshold = 4;        // 区分"点击吸附"与"拖拽选区"
} // namespace

OverlayWindow::OverlayWindow(const ScreenSnap& snap, SnipSession& session,
                             IElementLocator* elementLocator)
    : session_(session), elementLocator_(elementLocator), frozen_(snap.image),
      physical_(snap.image), dpr_(snap.dpr) {
    frozen_.setDevicePixelRatio(snap.dpr);
    elementThrottle_.start();

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

QRect OverlayWindow::selectedItemHandleRect() const {
    const AnnotationItem* item = session_.selectedItem();
    if (!item) {
        return {};
    }
    // 与选中虚线框一致的外扩;不与 rect() 相交裁剪,保证手柄命中完整
    return item->bounds().adjusted(-4, -4, 4, 4).translated(-geometry().topLeft());
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
        AnnotationRenderer::render(painter, session_.document(), &session_.snapshot());
        if (const AnnotationItem* pending = session_.pendingAnnotation()) {
            AnnotationRenderer::renderItem(painter, *pending, &session_.snapshot());
        }
        // 选中条目:虚线高亮框 + 编辑抓手(形状八向手柄/箭头端点圆点)
        if (const AnnotationItem* sel = session_.selectedItem()) {
            const QRect b = sel->bounds().adjusted(-4, -4, 4, 4);
            QPen dash(Qt::white, 1, Qt::DashLine);
            painter.setPen(dash);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(b);
            if (session_.selectedIsShape()) {
                SelectionHandles::paint(painter, b, Qt::white);
            } else if (session_.selectedIsArrow()) {
                const auto* arrow = static_cast<const ArrowItem*>(sel);
                painter.setPen(QPen(kBorderColor, 1));
                painter.setBrush(Qt::white);
                painter.drawEllipse(arrow->from, 4, 4);
                painter.drawEllipse(arrow->to, 4, 4);
            }
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

    // 标注工具激活时,选区内按下:点击型工具直接落子,拖拽型进入绘制
    if (session_.hasSelection() && session_.activeTool() &&
        session_.selection().contains(pressGlobal_)) {
        switch (*session_.activeTool()) {
        case AnnotationTool::Text:
            startTextEditing(pressGlobal_, /*editExisting=*/false);
            return;
        case AnnotationTool::Badge:
            addBadge(pressGlobal_);
            return;
        default:
            mode_ = Mode::Drawing;
            session_.beginAnnotation(pressGlobal_);
            return;
        }
    }

    if (session_.hasSelection()) {
        const SelectionHandles::Hit hit =
            SelectionHandles::hitTest(selectionLocal(), event->pos());
        if (hit == SelectionHandles::Hit::Inside) {
            // 已选中条目的编辑抓手优先:形状手柄缩放 / 箭头端点拖拽
            if (session_.selectedIsShape()) {
                const SelectionHandles::Hit itemHit =
                    SelectionHandles::hitTest(selectedItemHandleRect(), event->pos());
                if (itemHit != SelectionHandles::Hit::None &&
                    itemHit != SelectionHandles::Hit::Inside) {
                    mode_ = Mode::ResizingItem;
                    activeHandle_ = itemHit;
                    baseItemRect_ = session_.selectedItem()->bounds();
                    return;
                }
            } else if (session_.selectedIsArrow()) {
                const auto* arrow =
                    static_cast<const ArrowItem*>(session_.selectedItem());
                const bool nearFrom =
                    (pressGlobal_ - arrow->from).manhattanLength() <= 8;
                const bool nearTo = (pressGlobal_ - arrow->to).manhattanLength() <= 8;
                if (nearFrom || nearTo) {
                    mode_ = Mode::DraggingArrowEnd;
                    draggingArrowFrom_ = nearFrom;
                    baseArrowFrom_ = arrow->from;
                    baseArrowTo_ = arrow->to;
                    return;
                }
            }
            // 先尝试命中已有标注条目(顶层优先),否则移动选区
            if (session_.selectAnnotationAt(pressGlobal_)) {
                mode_ = Mode::DraggingItem;
                lastDragGlobal_ = pressGlobal_;
                itemDragTotal_ = QPoint();
                return;
            }
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
    case Mode::DraggingItem: {
        const QPoint delta = global - lastDragGlobal_;
        session_.moveSelectedAnnotation(delta);
        itemDragTotal_ += delta;
        lastDragGlobal_ = global;
        break;
    }
    case Mode::ResizingItem:
        session_.setSelectedShapeRect(
            SelectionHandles::resized(baseItemRect_, activeHandle_, global - pressGlobal_));
        break;
    case Mode::DraggingArrowEnd:
        session_.setSelectedArrowEndpoints(draggingArrowFrom_ ? global : baseArrowFrom_,
                                           draggingArrowFrom_ ? baseArrowTo_ : global);
        break;
    case Mode::Idle:
        if (session_.hasSelection()) {
            if (!session_.activeTool()) {
                // 选中条目的抓手光标优先于选区手柄光标
                SelectionHandles::Hit itemHit = SelectionHandles::Hit::None;
                if (session_.selectedIsShape()) {
                    itemHit =
                        SelectionHandles::hitTest(selectedItemHandleRect(), event->pos());
                } else if (session_.selectedIsArrow()) {
                    const auto* arrow =
                        static_cast<const ArrowItem*>(session_.selectedItem());
                    if ((global - arrow->from).manhattanLength() <= 8 ||
                        (global - arrow->to).manhattanLength() <= 8) {
                        setCursor(Qt::SizeAllCursor);
                        break;
                    }
                }
                if (itemHit != SelectionHandles::Hit::None &&
                    itemHit != SelectionHandles::Hit::Inside) {
                    setCursor(SelectionHandles::cursorFor(itemHit));
                } else {
                    setCursor(SelectionHandles::cursorFor(
                        SelectionHandles::hitTest(selectionLocal(), event->pos())));
                }
            }
        } else if (elementLocator_ && (event->modifiers() & Qt::ShiftModifier)) {
            updateElementHover(global); // Shift:吸附窗口内 UI 元素
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
    if (mode_ == Mode::DraggingItem) {
        session_.commitSelectedAnnotationMove(itemDragTotal_);
        mode_ = Mode::Idle;
        return;
    }
    if (mode_ == Mode::ResizingItem) {
        session_.commitSelectedShapeRect(baseItemRect_);
        mode_ = Mode::Idle;
        return;
    }
    if (mode_ == Mode::DraggingArrowEnd) {
        session_.commitSelectedArrow(baseArrowFrom_, baseArrowTo_);
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
    if (event->button() != Qt::LeftButton ||
        !session_.selection().contains(event->globalPosition().toPoint())) {
        return;
    }
    // 双击文字条目 → 二次编辑;否则双击 = 复制确认
    const QPoint global = event->globalPosition().toPoint();
    if (!session_.activeTool() && session_.selectAnnotationAt(global) &&
        session_.selectedIsText()) {
        const auto* text = static_cast<const TextItem*>(session_.selectedItem());
        startTextEditing(text->pos, /*editExisting=*/true);
        return;
    }
    session_.confirm();
}

void OverlayWindow::startTextEditing(const QPoint& globalPos, bool editExisting) {
    finishTextEditing(true); // 已有未提交文本先落盘

    // 二次编辑用条目自身样式,新建用当前默认样式
    const StrokeStyle style = editExisting && session_.selectedItem()
                                  ? session_.selectedItem()->style()
                                  : session_.strokeStyle();
    editingExistingText_ = editExisting;
    textPosGlobal_ = globalPos;
    textEditor_ = new QLineEdit(this);
    QFont font;
    font.setPixelSize(textPixelSizeFor(style));
    font.setBold(true);
    textEditor_->setFont(font);
    textEditor_->setStyleSheet(
        QStringLiteral("QLineEdit { background: rgba(0,0,0,140); color: %1;"
                       "  border: 1px dashed #AAA; padding: 1px 4px; }")
            .arg(style.color.name()));
    textEditor_->setMinimumWidth(120);
    textEditor_->move(globalPos - geometry().topLeft() - QPoint(4, 4));
    if (editExisting && session_.selectedIsText()) {
        textEditor_->setText(
            static_cast<const TextItem*>(session_.selectedItem())->text);
        textEditor_->selectAll();
    }
    textEditor_->installEventFilter(this);
    connect(textEditor_, &QLineEdit::returnPressed, this,
            [this] { finishTextEditing(true); });
    textEditor_->show();
    textEditor_->setFocus();
}

void OverlayWindow::finishTextEditing(bool accept) {
    if (!textEditor_) {
        return;
    }
    QLineEdit* editor = textEditor_;
    textEditor_ = nullptr; // 先置空,防 focus 链路重入
    const bool editExisting = editingExistingText_;
    editingExistingText_ = false;
    const QString text = editor->text().trimmed();
    editor->deleteLater();
    setFocus();
    if (!accept || text.isEmpty()) {
        return;
    }
    if (editExisting) {
        session_.editSelectedText(text);
    } else {
        session_.document().pushAddItem(std::make_unique<TextItem>(
            session_.strokeStyle(), textPosGlobal_, text));
    }
}

void OverlayWindow::addBadge(const QPoint& globalPos) {
    int number = 1;
    for (const auto& item : session_.document().items()) {
        if (item->tool() == AnnotationTool::Badge) {
            ++number;
        }
    }
    session_.document().pushAddItem(
        std::make_unique<BadgeItem>(session_.strokeStyle(), globalPos, number));
}

bool OverlayWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == textEditor_ && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            finishTextEditing(false);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        // 分层退出:标注工具 → 条目选中 → 取消会话
        if (session_.activeTool()) {
            session_.setActiveTool(std::nullopt);
        } else if (session_.selectedAnnotation() >= 0) {
            session_.clearAnnotationSelection();
        } else {
            session_.cancel();
        }
        break;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        session_.deleteSelectedAnnotation();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        session_.confirm();
        break;
    case Qt::Key_F3:
        session_.requestPin();
        break;
    case Qt::Key_C: {
        // 取色器:复制放大镜中心像素色值(C=HEX,Shift+C=RGB)
        if (!magnifierVisible()) {
            break;
        }
        QPoint phys(qRound(cursorLocal_.x() * dpr_), qRound(cursorLocal_.y() * dpr_));
        phys.setX(std::clamp(phys.x(), 0, physical_.width() - 1));
        phys.setY(std::clamp(phys.y(), 0, physical_.height() - 1));
        const QColor color = physical_.pixelColor(phys);
        session_.pickColor(event->modifiers() & Qt::ShiftModifier
                               ? QStringLiteral("%1, %2, %3")
                                     .arg(color.red())
                                     .arg(color.green())
                                     .arg(color.blue())
                               : color.name(QColor::HexRgb).toUpper());
        break;
    }
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
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        nudgeSelection(event);
        break;
    case Qt::Key_Shift:
        // 不动鼠标按下 Shift 也立即切到元素级高亮
        if (!event->isAutoRepeat() && elementLocator_ && !session_.hasSelection() &&
            hasCursor_ && mode_ == Mode::Idle) {
            updateElementHover(cursorLocal_ + geometry().topLeft(), /*force=*/true);
            update();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void OverlayWindow::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Shift && !event->isAutoRepeat() &&
        !session_.hasSelection() && hasCursor_ && mode_ == Mode::Idle) {
        session_.updateHover(cursorLocal_ + geometry().topLeft()); // 回窗口级
        update();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

// Shift 按住时的元素级吸附:经 UIA 在悬停窗口内下钻光标处元素。
// UIA 查询可能慢(几十 ms),节流 100ms;失败退回窗口级。
void OverlayWindow::updateElementHover(const QPoint& globalLogical, bool force) {
    if (!force && elementThrottle_.elapsed() < 100) {
        return; // 保持上一次的高亮
    }
    elementThrottle_.restart();
    const quintptr windowId = session_.windowIdAt(globalLogical);
    QRect rect;
    if (windowId) {
        rect = elementLocator_->elementAt(windowId, globalLogical);
    }
    if (rect.isValid() && rect.contains(globalLogical) && rect.width() >= 8 &&
        rect.height() >= 8) {
        session_.setHoverRect(rect);
    } else {
        session_.updateHover(globalLogical); // 退回窗口级
    }
}

// 方向键微调选区:移动 1px;Ctrl=调整大小(右/下边);Shift=步长x10。
// 配合放大镜做像素级精确选区。
void OverlayWindow::nudgeSelection(QKeyEvent* event) {
    if (!session_.hasSelection() || session_.activeTool()) {
        return;
    }
    const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    int dx = 0;
    int dy = 0;
    switch (event->key()) {
    case Qt::Key_Left: dx = -step; break;
    case Qt::Key_Right: dx = step; break;
    case Qt::Key_Up: dy = -step; break;
    case Qt::Key_Down: dy = step; break;
    default: return;
    }

    const QRect bounds = session_.snapshot().virtualGeometryLogical();
    QRect sel = session_.selection();
    if (event->modifiers() & Qt::ControlModifier) {
        // 调整大小:动右边和下边,保持左上角不动
        sel.setRight(std::clamp(sel.right() + dx, sel.left(), bounds.right()));
        sel.setBottom(std::clamp(sel.bottom() + dy, sel.top(), bounds.bottom()));
    } else {
        QPoint topLeft = sel.topLeft() + QPoint(dx, dy);
        topLeft.setX(std::clamp(topLeft.x(), bounds.left(),
                                bounds.right() - sel.width() + 1));
        topLeft.setY(std::clamp(topLeft.y(), bounds.top(),
                                bounds.bottom() - sel.height() + 1));
        sel.moveTopLeft(topLeft);
    }
    session_.setSelection(sel);
}

} // namespace pixora
