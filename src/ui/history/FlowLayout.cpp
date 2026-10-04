#include "ui/history/FlowLayout.h"

#include <QWidget>

#include <algorithm>

namespace pixora {

FlowLayout::FlowLayout(QWidget* parent, int spacing) : QLayout(parent), spacing_(spacing) {
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout() {
    while (QLayoutItem* item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem* item) {
    items_.append(item);
}

int FlowLayout::count() const {
    return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return items_.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index) {
    return index >= 0 && index < items_.size() ? items_.takeAt(index) : nullptr;
}

int FlowLayout::heightForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), /*testOnly=*/true);
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem* item : items_) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, /*testOnly=*/false);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
    const QMargins m = contentsMargins();
    const QRect area = rect.marginsRemoved(m);
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;
    for (QLayoutItem* item : items_) {
        const QSize hint = item->sizeHint();
        if (x + hint.width() > area.right() + 1 && lineHeight > 0) {
            x = area.x();
            y += lineHeight + spacing_;
            lineHeight = 0;
        }
        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), hint));
        }
        x += hint.width() + spacing_;
        lineHeight = std::max(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + m.bottom();
}

} // namespace pixora
