#include "core/capture/SnipSession.h"

#include <algorithm>
#include <utility>

namespace pixora {

SnipSession::SnipSession(DesktopSnapshot snapshot, QObject* parent)
    : QObject(parent), snapshot_(std::move(snapshot)) {}

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

void SnipSession::cancel() {
    emit cancelled();
}

} // namespace pixora
