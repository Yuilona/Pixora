#include "core/capture/SnipSession.h"

#include <algorithm>
#include <utility>

namespace pixora {

SnipSession::SnipSession(DesktopSnapshot snapshot, QObject* parent)
    : QObject(parent), snapshot_(std::move(snapshot)) {
    connect(&document_, &AnnotationDocument::changed, this,
            &SnipSession::annotationsChanged);
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

void SnipSession::cancel() {
    emit cancelled();
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
        pending_ = std::make_unique<ShapeItem>(*tool_, style_, QRect(globalPos, globalPos));
        break;
    case AnnotationTool::Arrow:
        pending_ = std::make_unique<ArrowItem>(style_, globalPos, globalPos);
        break;
    case AnnotationTool::Pen:
        pending_ = std::make_unique<PenItem>(style_, QPolygon{globalPos});
        break;
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
        static_cast<ShapeItem*>(pending_.get())->rect.setBottomRight(globalPos);
        break;
    case AnnotationTool::Arrow:
        static_cast<ArrowItem*>(pending_.get())->to = globalPos;
        break;
    case AnnotationTool::Pen: {
        QPolygon& pts = static_cast<PenItem*>(pending_.get())->points;
        if (pts.isEmpty() || (globalPos - pts.last()).manhattanLength() >= 2) {
            pts << globalPos;
        }
        break;
    }
    }
    emit annotationsChanged();
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
