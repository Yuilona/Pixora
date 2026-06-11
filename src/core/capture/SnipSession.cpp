#include "core/capture/SnipSession.h"

#include <algorithm>
#include <utility>

namespace pixora {

SnipSession::SnipSession(DesktopSnapshot snapshot, QObject* parent)
    : QObject(parent), snapshot_(std::move(snapshot)) {
    connect(&document_, &AnnotationDocument::changed, this, [this] {
        // 撤销/重做可能使选中索引失效
        if (selectedAnnotation_ >= static_cast<int>(document_.items().size())) {
            selectedAnnotation_ = -1;
        }
        emit annotationsChanged();
    });
}

bool SnipSession::hasSelection() const {
    return selection_.width() >= 1 && selection_.height() >= 1;
}

void SnipSession::setSelection(const QRect& rect) {
    // 视 rect 两角为拖拽锚点,取 min/max 规整——两个锚点像素都含在选区内。
    // (不用 QRect::normalized():Qt6 下它保持尺寸,翻转时会整体偏移 1px)
    const QRect normalized(
        QPoint(std::min(rect.left(), rect.right()), std::min(rect.top(), rect.bottom())),
        QPoint(std::max(rect.left(), rect.right()), std::max(rect.top(), rect.bottom())));
    if (selection_ == normalized) {
        return;
    }
    selection_ = normalized;
    emit selectionChanged(selection_);
}

void SnipSession::setWindowCandidates(std::vector<WindowInfo> windows) {
    candidates_ = std::move(windows);
}

void SnipSession::updateHover(const QPoint& globalLogical) {
    QRect hit;
    for (const WindowInfo& w : candidates_) {
        if (w.frameLogical.contains(globalLogical)) {
            hit = w.frameLogical;
            break;
        }
    }
    if (hover_ == hit) {
        return;
    }
    hover_ = hit;
    emit hoverChanged(hover_);
}

void SnipSession::confirm() {
    if (hasSelection()) {
        emit confirmed(selection_);
    }
}

void SnipSession::requestSave() {
    if (hasSelection()) {
        emit saveRequested(selection_);
    }
}

void SnipSession::requestPin() {
    if (hasSelection()) {
        emit pinRequested(selection_);
    }
}

void SnipSession::requestScroll() {
    if (hasSelection()) {
        emit scrollRequested(selection_);
    }
}

void SnipSession::cancel() {
    emit cancelled();
}

void SnipSession::pickColor(const QString& text) {
    emit colorPicked(text);
}

void SnipSession::notifyInteractionFinished() {
    if (hasSelection()) {
        emit interactionFinished();
    }
}

void SnipSession::setActiveTool(std::optional<AnnotationTool> tool) {
    if (tool_ == tool) {
        return;
    }
    tool_ = tool;
    emit activeToolChanged();
}

void SnipSession::beginAnnotation(const QPoint& globalPos) {
    if (!tool_) {
        return;
    }
    switch (*tool_) {
    case AnnotationTool::Rect:
    case AnnotationTool::Ellipse:
    case AnnotationTool::Mosaic:
    case AnnotationTool::Blur:
        pending_ = std::make_unique<ShapeItem>(*tool_, style_, QRect(globalPos, globalPos));
        break;
    case AnnotationTool::Arrow:
        pending_ = std::make_unique<ArrowItem>(style_, globalPos, globalPos);
        break;
    case AnnotationTool::Pen:
    case AnnotationTool::Marker:
        pending_ = std::make_unique<PenItem>(*tool_, style_, QPolygon{globalPos});
        break;
    case AnnotationTool::Text:
    case AnnotationTool::Badge:
        return; // 点击型工具,由遮罩窗直接提交,不走拖拽流程
    }
    emit annotationsChanged();
}

void SnipSession::updateAnnotation(const QPoint& globalPos) {
    if (!pending_) {
        return;
    }
    switch (pending_->tool()) {
    case AnnotationTool::Rect:
    case AnnotationTool::Ellipse:
    case AnnotationTool::Mosaic:
    case AnnotationTool::Blur:
        static_cast<ShapeItem*>(pending_.get())->rect.setBottomRight(globalPos);
        break;
    case AnnotationTool::Arrow:
        static_cast<ArrowItem*>(pending_.get())->to = globalPos;
        break;
    case AnnotationTool::Pen:
    case AnnotationTool::Marker: {
        QPolygon& pts = static_cast<PenItem*>(pending_.get())->points;
        if (pts.isEmpty() || (globalPos - pts.last()).manhattanLength() >= 2) {
            pts << globalPos;
        }
        break;
    }
    case AnnotationTool::Text:
    case AnnotationTool::Badge:
        break;
    }
    emit annotationsChanged();
}

bool SnipSession::selectAnnotationAt(const QPoint& globalPos) {
    const auto& items = document_.items();
    for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) { // 顶层优先
        if (items[static_cast<size_t>(i)]->hitTest(globalPos)) {
            if (selectedAnnotation_ != i) {
                selectedAnnotation_ = i;
                emit annotationsChanged();
            }
            return true;
        }
    }
    clearAnnotationSelection();
    return false;
}

void SnipSession::clearAnnotationSelection() {
    if (selectedAnnotation_ != -1) {
        selectedAnnotation_ = -1;
        emit annotationsChanged();
    }
}

void SnipSession::moveSelectedAnnotation(const QPoint& delta) {
    if (selectedAnnotation_ < 0 || delta.isNull()) {
        return;
    }
    document_.items()[static_cast<size_t>(selectedAnnotation_)]->translate(delta);
    emit annotationsChanged();
}

void SnipSession::commitSelectedAnnotationMove(const QPoint& totalDelta) {
    document_.pushMoveItem(selectedAnnotation_, totalDelta);
}

void SnipSession::deleteSelectedAnnotation() {
    if (selectedAnnotation_ < 0) {
        return;
    }
    const int index = selectedAnnotation_;
    selectedAnnotation_ = -1;
    document_.pushRemoveItem(index);
}

const AnnotationItem* SnipSession::selectedItem() const {
    if (selectedAnnotation_ < 0 ||
        selectedAnnotation_ >= static_cast<int>(document_.items().size())) {
        return nullptr;
    }
    return document_.items()[static_cast<size_t>(selectedAnnotation_)].get();
}

bool SnipSession::selectedIsShape() const {
    const AnnotationItem* item = selectedItem();
    if (!item) {
        return false;
    }
    switch (item->tool()) {
    case AnnotationTool::Rect:
    case AnnotationTool::Ellipse:
    case AnnotationTool::Mosaic:
    case AnnotationTool::Blur:
        return true;
    default:
        return false;
    }
}

bool SnipSession::selectedIsArrow() const {
    const AnnotationItem* item = selectedItem();
    return item && item->tool() == AnnotationTool::Arrow;
}

bool SnipSession::selectedIsText() const {
    const AnnotationItem* item = selectedItem();
    return item && item->tool() == AnnotationTool::Text;
}

void SnipSession::setSelectedShapeRect(const QRect& rect) {
    if (!selectedIsShape()) {
        return;
    }
    auto& shape = static_cast<ShapeItem&>(
        *document_.items()[static_cast<size_t>(selectedAnnotation_)]);
    if (shape.rect == rect) {
        return;
    }
    shape.rect = rect;
    emit annotationsChanged();
}

void SnipSession::setSelectedArrowEndpoints(const QPoint& from, const QPoint& to) {
    if (!selectedIsArrow()) {
        return;
    }
    auto& arrow = static_cast<ArrowItem&>(
        *document_.items()[static_cast<size_t>(selectedAnnotation_)]);
    if (arrow.from == from && arrow.to == to) {
        return;
    }
    arrow.from = from;
    arrow.to = to;
    emit annotationsChanged();
}

void SnipSession::commitSelectedShapeRect(const QRect& oldRect) {
    document_.pushSetShapeRect(selectedAnnotation_, oldRect);
}

void SnipSession::commitSelectedArrow(const QPoint& oldFrom, const QPoint& oldTo) {
    document_.pushSetArrow(selectedAnnotation_, oldFrom, oldTo);
}

void SnipSession::chooseColor(const QColor& color) {
    style_.color = color;
    if (const AnnotationItem* item = selectedItem()) {
        StrokeStyle patched = item->style(); // 只动颜色,保留条目自身粗细
        patched.color = color;
        document_.pushRestyleItem(selectedAnnotation_, patched);
    }
}

void SnipSession::chooseWidth(int width) {
    style_.width = width;
    if (const AnnotationItem* item = selectedItem()) {
        StrokeStyle patched = item->style(); // 只动粗细,保留条目自身颜色
        patched.width = width;
        document_.pushRestyleItem(selectedAnnotation_, patched);
    }
}

void SnipSession::editSelectedText(const QString& text) {
    if (selectedIsText()) {
        document_.pushEditText(selectedAnnotation_, text);
    }
}

void SnipSession::endAnnotation() {
    if (!pending_) {
        return;
    }
    // 无位移的点击不产生条目(画笔除外:单点视为画点,仍丢弃过小者)
    const QRect b = pending_->bounds();
    if (b.width() < 2 && b.height() < 2) {
        pending_.reset();
    } else {
        document_.pushAddItem(std::move(pending_));
    }
    emit annotationsChanged();
}

} // namespace pixora
