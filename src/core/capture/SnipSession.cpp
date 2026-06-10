#include "core/capture/SnipSession.h"

#include <utility>

namespace pixora {

SnipSession::SnipSession(DesktopSnapshot snapshot, QObject* parent)
    : QObject(parent), snapshot_(std::move(snapshot)) {}

bool SnipSession::hasSelection() const {
    return selection_.width() >= 1 && selection_.height() >= 1;
}

void SnipSession::setSelection(const QRect& rect) {
    const QRect normalized = rect.normalized();
    if (selection_ == normalized) {
        return;
    }
    selection_ = normalized;
    emit selectionChanged(selection_);
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
