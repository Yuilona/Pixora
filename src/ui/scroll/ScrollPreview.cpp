#include "ui/scroll/ScrollPreview.h"

#include "ui/Theme.h"

#include <QPainter>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kWidth = 168;
constexpr int kGap = 12;      // 与捕获区域的间距
constexpr int kHeaderH = 24;  // 顶部"已拼高度"信息条
constexpr int kPadding = 6;

// 区域右侧优先,放不下换左侧;都放不下返回空
QRect placeBeside(const QRect& region, const QRect& bounds) {
    const int h = std::clamp(region.height(), 240, 560);
    const int y = std::clamp(region.top(), bounds.top(),
                             std::max(bounds.top(), bounds.bottom() - h + 1));
    const QRect right(region.right() + 1 + kGap, y, kWidth, h);
    if (bounds.contains(right)) {
        return right;
    }
    const QRect left(region.left() - kGap - kWidth, y, kWidth, h);
    if (bounds.contains(left)) {
        return left;
    }
    return {};
}

} // namespace

bool ScrollPreview::fitsBeside(const QRect& regionGlobal, const QRect& virtualBounds) {
    return !placeBeside(regionGlobal, virtualBounds).isNull();
}

ScrollPreview::ScrollPreview(const QRect& regionGlobal, const QRect& virtualBounds) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setGeometry(placeBeside(regionGlobal, virtualBounds));
}

int ScrollPreview::tailRowsFor(int canvasWidthPhysical) const {
    const int contentW = width() - 2 * kPadding;
    const int contentH = height() - kHeaderH - 2 * kPadding;
    if (contentW <= 0 || canvasWidthPhysical <= 0) {
        return 0;
    }
    // 内容区能展示的画布行数 = 内容区高 / 缩放比(预览宽/画布宽)
    return contentH * canvasWidthPhysical / contentW;
}

void ScrollPreview::updateContent(const QImage& tail, int totalLogicalHeight) {
    totalLogicalHeight_ = totalLogicalHeight;
    if (!tail.isNull()) {
        const qreal dpr = devicePixelRatio();
        const int targetW = qRound((width() - 2 * kPadding) * dpr);
        scaled_ = QPixmap::fromImage(
            tail.scaledToWidth(std::max(1, targetW), Qt::SmoothTransformation));
        scaled_.setDevicePixelRatio(dpr);
    }
    update();
}

void ScrollPreview::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    QColor bg = theme::surface();
    bg.setAlpha(235);
    painter.fillRect(rect(), bg);
    painter.setPen(theme::accent());
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont font = painter.font();
    font.setPixelSize(11);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(kPadding, 0, width() - 2 * kPadding, kHeaderH),
                     Qt::AlignVCenter,
                     tr("Stitched %1 px").arg(totalLogicalHeight_));

    if (scaled_.isNull()) {
        painter.setPen(QColor(255, 255, 255, 120));
        painter.drawText(rect().adjusted(kPadding, kHeaderH, -kPadding, -kPadding),
                         Qt::AlignCenter, tr("Waiting for the first frame..."));
        return;
    }
    // 底部对齐:始终展示最新拼接的部分
    const QRect content(kPadding, kHeaderH + kPadding, width() - 2 * kPadding,
                        height() - kHeaderH - 2 * kPadding);
    const QSizeF logical = scaled_.deviceIndependentSize();
    const int drawH = std::min<int>(qRound(logical.height()), content.height());
    const QPoint topLeft(content.left(), content.bottom() + 1 - drawH);
    // 源矩形取缩放图的尾部(设备像素)
    const qreal dpr = scaled_.devicePixelRatio();
    const QRectF source(0, scaled_.height() - drawH * dpr, scaled_.width(), drawH * dpr);
    painter.drawPixmap(QRectF(topLeft, QSizeF(content.width(), drawH)), scaled_, source);
}

} // namespace pixora
