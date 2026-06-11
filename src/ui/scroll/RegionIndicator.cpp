#include "ui/scroll/RegionIndicator.h"

#include <QPainter>

namespace pixora {

namespace {
const QColor kFrameColor(0xFF, 0x98, 0x00); // 橙色,与截图蓝区分
constexpr int kBorder = 3;
} // namespace

RegionIndicator::RegionIndicator(const QRect& regionGlobal) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(regionGlobal.adjusted(-kBorder, -kBorder, kBorder, kBorder));
}

void RegionIndicator::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    QPen pen(kFrameColor, 2, Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
}

} // namespace pixora
