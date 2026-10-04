#include "ui/overlay/Magnifier.h"

#include "ui/Theme.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace pixora {
namespace Magnifier {

namespace {
constexpr int kGrabW = 21; // 物理像素取样宽(奇数,有中心点)
constexpr int kGrabH = 15;
constexpr int kZoom = 8;
constexpr int kInset = 4;       // 放大画面距卡片边
constexpr int kInfoHeight = 46; // 两行信息栏
constexpr int kCursorGap = 24;  // 卡片距光标
} // namespace

void draw(QPainter& painter, const Context& ctx) {
    if (!ctx.physicalImage || ctx.physicalImage->isNull()) {
        return;
    }
    const QImage& img = *ctx.physicalImage;

    QPoint phys(qRound(ctx.cursorLocalLogical.x() * ctx.dpr),
                qRound(ctx.cursorLocalLogical.y() * ctx.dpr));
    phys.setX(std::clamp(phys.x(), 0, img.width() - 1));
    phys.setY(std::clamp(phys.y(), 0, img.height() - 1));

    // 取样区贴边时整体平移,保证尺寸恒定
    QRect grab(phys.x() - kGrabW / 2, phys.y() - kGrabH / 2, kGrabW, kGrabH);
    grab.moveLeft(std::clamp(grab.left(), 0, img.width() - kGrabW));
    grab.moveTop(std::clamp(grab.top(), 0, img.height() - kGrabH));

    const QSize zoomSize(kGrabW * kZoom, kGrabH * kZoom);
    const QSize cardSize(zoomSize.width() + 2 * kInset,
                         kInset + zoomSize.height() + kInfoHeight);

    // 默认在光标右下,越界则翻到另一侧
    QPoint topLeft = ctx.cursorLocalLogical + QPoint(kCursorGap, kCursorGap);
    if (topLeft.x() + cardSize.width() > ctx.widgetSize.width()) {
        topLeft.setX(ctx.cursorLocalLogical.x() - kCursorGap - cardSize.width());
    }
    if (topLeft.y() + cardSize.height() > ctx.widgetSize.height()) {
        topLeft.setY(ctx.cursorLocalLogical.y() - kCursorGap - cardSize.height());
    }
    const QRect card(topLeft, cardSize);

    painter.save();
    theme::paintShadow(painter, card, theme::corner::card);
    theme::paintCard(painter, card, theme::corner::card);

    // —— 放大画面(邻近插值,圆角裁剪)——
    const QRect zoomRect(card.topLeft() + QPoint(kInset, kInset), zoomSize);
    QPainterPath zoomClip;
    zoomClip.addRoundedRect(QRectF(zoomRect), theme::corner::chip, theme::corner::chip);
    painter.setClipPath(zoomClip);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(zoomRect, img, grab);

    // 像素网格:极淡,只在放大后提示像素边界
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(0, 0, 0, 26), 1));
    for (int i = 1; i < kGrabW; ++i) {
        const int x = zoomRect.left() + i * kZoom;
        painter.drawLine(x, zoomRect.top(), x, zoomRect.bottom());
    }
    for (int i = 1; i < kGrabH; ++i) {
        const int y = zoomRect.top() + i * kZoom;
        painter.drawLine(zoomRect.left(), y, zoomRect.right(), y);
    }

    // 十字准星:中心行列淡蓝色带,避开中心格——被取色的像素本身不加任何遮盖
    const QPoint cell = phys - grab.topLeft();
    const QRect cellRect(zoomRect.x() + cell.x() * kZoom, zoomRect.y() + cell.y() * kZoom,
                         kZoom, kZoom);
    QColor band = theme::accent();
    band.setAlpha(60);
    painter.fillRect(QRect(zoomRect.left(), cellRect.top(), cellRect.left() - zoomRect.left(),
                           kZoom),
                     band);
    painter.fillRect(QRect(cellRect.right() + 1, cellRect.top(),
                           zoomRect.right() - cellRect.right(), kZoom),
                     band);
    painter.fillRect(QRect(cellRect.left(), zoomRect.top(), kZoom,
                           cellRect.top() - zoomRect.top()),
                     band);
    painter.fillRect(QRect(cellRect.left(), cellRect.bottom() + 1, kZoom,
                           zoomRect.bottom() - cellRect.bottom()),
                     band);
    // 中心格:黑白双框,任何底色上都能定位
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(cellRect.adjusted(-1, -1, 0, 0));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(cellRect.adjusted(0, 0, -1, -1));
    painter.setClipping(false);
    painter.setRenderHint(QPainter::Antialiasing);

    // —— 信息栏:色块 + HEX + RGB / 坐标 + 取色提示 ——
    const QColor color = img.pixelColor(phys);
    const int left = card.left() + kInset + 4;
    const int right = card.right() - kInset - 4;
    const QRect row1(left, zoomRect.bottom() + 5, right - left, 20);
    const QRect row2(left, row1.bottom() + 1, right - left, 16);

    const QRect swatch(row1.left(), row1.center().y() - 6, 12, 12);
    painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(swatch).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);

    painter.setFont(theme::font(theme::fontsize::label, /*bold=*/true));
    painter.setPen(theme::text());
    painter.drawText(row1.adjusted(swatch.width() + 7, 0, 0, 0), Qt::AlignVCenter,
                     color.name(QColor::HexRgb).toUpper());
    painter.setFont(theme::font(theme::fontsize::caption));
    painter.setPen(theme::textDim());
    painter.drawText(row1, Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("%1, %2, %3")
                         .arg(color.red())
                         .arg(color.green())
                         .arg(color.blue()));

    painter.setPen(theme::textFaint());
    painter.drawText(row2, Qt::AlignVCenter,
                     QStringLiteral("%1, %2")
                         .arg(ctx.cursorGlobalLogical.x())
                         .arg(ctx.cursorGlobalLogical.y()));
    painter.drawText(row2, Qt::AlignVCenter | Qt::AlignRight,
                     QCoreApplication::translate("Magnifier", "C to pick color"));

    painter.restore();
}

} // namespace Magnifier
} // namespace pixora
