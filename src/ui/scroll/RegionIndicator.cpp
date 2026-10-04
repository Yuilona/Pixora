#include "ui/scroll/RegionIndicator.h"

#include "ui/Theme.h"

#include <QPainter>

namespace pixora {

namespace {
// 边框带宽:全部画在捕获区域之外,绝不进入抓帧范围
constexpr int kBorder = 4;
constexpr int kCorner = 16; // 角标臂长
} // namespace

RegionIndicator::RegionIndicator(const QRect& regionGlobal) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(regionGlobal.adjusted(-kBorder, -kBorder, kBorder, kBorder));
}

void RegionIndicator::paintEvent(QPaintEvent* /*event*/) {
    // 琥珀色(与截图选区蓝区分,示意"录制中"):外带 1.5px 实线 + 四角加粗角标
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = theme::warning();
    const QRectF outer = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
    painter.setPen(QPen(color, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(outer);

    // 角标:L 形,厚度 = 整个边框带,压在区域外沿
    const int w = width();
    const int h = height();
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const QRect& arm : {
             QRect(0, 0, kCorner, kBorder), QRect(0, 0, kBorder, kCorner),
             QRect(w - kCorner, 0, kCorner, kBorder), QRect(w - kBorder, 0, kBorder, kCorner),
             QRect(0, h - kBorder, kCorner, kBorder), QRect(0, h - kCorner, kBorder, kCorner),
             QRect(w - kCorner, h - kBorder, kCorner, kBorder),
             QRect(w - kBorder, h - kCorner, kBorder, kCorner)}) {
        painter.fillRect(arm, color);
    }
}

} // namespace pixora
