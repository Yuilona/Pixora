#include "ui/overlay/Magnifier.h"

#include <QCoreApplication>
#include <QPainter>

#include <algorithm>

namespace pixora {
namespace Magnifier {

namespace {
constexpr int kGrabW = 21; // 物理像素取样宽(奇数,有中心点)
constexpr int kGrabH = 15;
constexpr int kZoom = 8;
constexpr int kInfoHeight = 36;
const QColor kCrosshair(45, 124, 246, 160);
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
    const QSize boxSize(zoomSize.width(), zoomSize.height() + kInfoHeight);

    // 默认在光标右下,越界则翻到另一侧
    QPoint topLeft = ctx.cursorLocalLogical + QPoint(24, 24);
    if (topLeft.x() + boxSize.width() > ctx.widgetSize.width()) {
        topLeft.setX(ctx.cursorLocalLogical.x() - 24 - boxSize.width());
    }
    if (topLeft.y() + boxSize.height() > ctx.widgetSize.height()) {
        topLeft.setY(ctx.cursorLocalLogical.y() - 24 - boxSize.height());
    }

    painter.save();

    // 放大画面(邻近插值)
    const QRect zoomRect(topLeft, zoomSize);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(zoomRect, img, grab);

    // 十字线指向取样中心像素
    const QPoint cell = phys - grab.topLeft();
    const QRect cellRect(zoomRect.x() + cell.x() * kZoom, zoomRect.y() + cell.y() * kZoom,
                         kZoom, kZoom);
    painter.setPen(QPen(kCrosshair, kZoom));
    painter.drawLine(QPoint(cellRect.center().x(), zoomRect.top()),
                     QPoint(cellRect.center().x(), zoomRect.bottom()));
    painter.drawLine(QPoint(zoomRect.left(), cellRect.center().y()),
                     QPoint(zoomRect.right(), cellRect.center().y()));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(cellRect);

    // 信息栏:坐标 + 色值
    const QColor color = img.pixelColor(phys);
    const QRect infoRect(topLeft + QPoint(0, zoomSize.height()),
                         QSize(boxSize.width(), kInfoHeight));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 200));
    painter.drawRect(infoRect);

    QFont font = painter.font();
    font.setPixelSize(11);
    painter.setFont(font);
    painter.setPen(Qt::white);
    const QString posText = QStringLiteral("POS (%1, %2)")
                                .arg(ctx.cursorGlobalLogical.x())
                                .arg(ctx.cursorGlobalLogical.y());
    const QString rgbText = color.name(QColor::HexRgb).toUpper();
    painter.drawText(infoRect.adjusted(8, 2, -8, -kInfoHeight / 2), Qt::AlignVCenter,
                     posText);
    painter.drawText(infoRect.adjusted(8 + 14, kInfoHeight / 2, -8, -2), Qt::AlignVCenter,
                     rgbText);
    painter.setPen(QColor(255, 255, 255, 140));
    painter.drawText(infoRect.adjusted(8, kInfoHeight / 2, -8, -2),
                     Qt::AlignVCenter | Qt::AlignRight,
                     QCoreApplication::translate("Magnifier", "C to pick color"));
    painter.setPen(Qt::white);
    painter.setBrush(color);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawRect(QRect(infoRect.left() + 8, infoRect.top() + kInfoHeight / 2 + 4, 9, 9));

    // 外框
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 200), 1));
    painter.drawRect(QRect(topLeft, boxSize).adjusted(0, 0, -1, -1));

    painter.restore();
}

} // namespace Magnifier
} // namespace pixora
