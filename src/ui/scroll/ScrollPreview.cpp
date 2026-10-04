#include "ui/scroll/ScrollPreview.h"

#include "ui/Theme.h"

#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kWidth = 168;   // 卡片宽(不含投影边距)
constexpr int kGap = 12;      // 卡片与捕获区域的间距(投影外扩 10px,不会压进区域)
constexpr int kHeaderH = 28;  // 顶部"已拼高度"信息条
constexpr int kPadding = 6;
constexpr int kFadeH = 18;    // 内容超出时顶部渐隐,暗示上方还有

// 卡片位置:区域右侧优先,放不下换左侧;都放不下返回空
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
    setAttribute(Qt::WA_TranslucentBackground); // 圆角卡片 + 投影
    constexpr int m = theme::kShadowMargin;
    setGeometry(placeBeside(regionGlobal, virtualBounds).adjusted(-m, -m, m, m));
}

QRect ScrollPreview::cardRect() const {
    constexpr int m = theme::kShadowMargin;
    return rect().adjusted(m, m, -m, -m);
}

QRect ScrollPreview::contentRect() const {
    return cardRect().adjusted(kPadding, kHeaderH, -kPadding, -kPadding);
}

int ScrollPreview::tailRowsFor(int canvasWidthPhysical) const {
    const QRect content = contentRect();
    if (content.width() <= 0 || canvasWidthPhysical <= 0) {
        return 0;
    }
    // 内容区能展示的画布行数 = 内容区高 / 缩放比(预览宽/画布宽)
    return content.height() * canvasWidthPhysical / content.width();
}

void ScrollPreview::updateContent(const QImage& tail, int totalLogicalHeight) {
    totalLogicalHeight_ = totalLogicalHeight;
    if (!tail.isNull()) {
        const qreal dpr = devicePixelRatio();
        const int targetW = qRound(contentRect().width() * dpr);
        scaled_ = QPixmap::fromImage(
            tail.scaledToWidth(std::max(1, targetW), Qt::SmoothTransformation));
        scaled_.setDevicePixelRatio(dpr);
    }
    update();
}

void ScrollPreview::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    const QRect card = cardRect();
    theme::paintShadow(painter, card, theme::corner::card);
    theme::paintCard(painter, card, theme::corner::card, /*bgAlpha=*/245);

    // 表头:说明文字 + 已拼高度
    const QRect header(card.left() + kPadding + 2, card.top(), card.width() - 2 * kPadding - 4,
                       kHeaderH);
    painter.setFont(theme::font(theme::fontsize::caption));
    painter.setPen(theme::textFaint());
    painter.drawText(header, Qt::AlignVCenter | Qt::AlignLeft, tr("Preview"));
    painter.setFont(theme::font(theme::fontsize::body, /*bold=*/true));
    painter.setPen(theme::text());
    painter.drawText(header, Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("%1 px").arg(QLocale().toString(totalLogicalHeight_)));

    const QRect content = contentRect();
    if (scaled_.isNull()) {
        painter.setFont(theme::font(theme::fontsize::caption));
        painter.setPen(theme::textFaint());
        painter.drawText(content, Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Waiting for the first frame..."));
        return;
    }

    QPainterPath clip;
    clip.addRoundedRect(QRectF(content), theme::corner::chip, theme::corner::chip);
    painter.setClipPath(clip);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // 底部对齐:始终展示最新拼接的部分
    const QSizeF logical = scaled_.deviceIndependentSize();
    const int drawH = std::min<int>(qRound(logical.height()), content.height());
    const QPoint topLeft(content.left(), content.bottom() + 1 - drawH);
    // 源矩形取缩放图的尾部(设备像素)
    const qreal dpr = scaled_.devicePixelRatio();
    const QRectF source(0, scaled_.height() - drawH * dpr, scaled_.width(), drawH * dpr);
    painter.drawPixmap(QRectF(topLeft, QSizeF(content.width(), drawH)), scaled_, source);

    if (logical.height() > content.height()) {
        QColor top = theme::surface();
        QColor clear = top;
        clear.setAlpha(0);
        QLinearGradient fade(content.topLeft(), content.topLeft() + QPoint(0, kFadeH));
        fade.setColorAt(0, top);
        fade.setColorAt(1, clear);
        painter.fillRect(QRect(content.left(), content.top(), content.width(), kFadeH), fade);
    }
}

} // namespace pixora
