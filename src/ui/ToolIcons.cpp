#include "ui/ToolIcons.h"

#include "ui/Theme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <algorithm>
#include <functional>

namespace pixora::icons {

namespace {

const QColor kIconColor(0xDD, 0xDD, 0xDD);

QIcon makeIcon(const std::function<void(QPainter&)>& draw) {
    // 逻辑 20x20、2x 渲染;绘制代码仍用 16 单位坐标系,整体缩放适配
    QPixmap pm(40, 40);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(20.0 / 16.0, 20.0 / 16.0);
    p.setPen(QPen(kIconColor, 1.6));
    p.setBrush(Qt::NoBrush);
    draw(p);
    p.end();

    QIcon icon(pm);
    // 禁用态:同图 35% 透明(Qt 自动生成的灰化版在深色底上几乎不可辨)
    QPixmap dim(40, 40);
    dim.setDevicePixelRatio(2.0);
    dim.fill(Qt::transparent);
    QPainter dp(&dim);
    dp.setOpacity(0.35);
    dp.drawPixmap(0, 0, pm);
    dp.end();
    icon.addPixmap(dim, QIcon::Disabled);
    return icon;
}

} // namespace

QIcon toolIcon(AnnotationTool tool) {
    switch (tool) {
    case AnnotationTool::Rect:
        return makeIcon([](QPainter& p) { p.drawRect(QRectF(2.5, 3.5, 11, 9)); });
    case AnnotationTool::Ellipse:
        return makeIcon([](QPainter& p) { p.drawEllipse(QRectF(2.5, 3, 11, 10)); });
    case AnnotationTool::Arrow:
        return makeIcon([](QPainter& p) {
            p.drawLine(QPointF(3.5, 12.5), QPointF(12, 4));
            QPainterPath head(QPointF(12.5, 3.5));
            head.lineTo(QPointF(8.5, 4.5));
            head.lineTo(QPointF(11.5, 7.5));
            head.closeSubpath();
            p.fillPath(head, kIconColor);
        });
    case AnnotationTool::Pen:
        return makeIcon([](QPainter& p) {
            // 斜置铅笔:描边笔身 + 实心笔尖 + 尾部橡皮分隔线
            p.setPen(QPen(kIconColor, 1.2, Qt::SolidLine, Qt::RoundCap,
                          Qt::RoundJoin));
            QPainterPath body;
            body.moveTo(QPointF(3.4, 10.2));
            body.lineTo(QPointF(10.8, 2.8));
            body.lineTo(QPointF(13.2, 5.2));
            body.lineTo(QPointF(5.8, 12.6));
            body.closeSubpath();
            p.drawPath(body);
            QPainterPath tip(QPointF(2.4, 13.6)); // 笔尖收于左下
            tip.lineTo(QPointF(3.4, 10.2));
            tip.lineTo(QPointF(5.8, 12.6));
            tip.closeSubpath();
            p.fillPath(tip, kIconColor);
            p.drawLine(QPointF(9.2, 4.4), QPointF(11.6, 6.8)); // 橡皮分隔
        });
    case AnnotationTool::Marker:
        return makeIcon([](QPainter& p) {
            QColor c = kIconColor;
            c.setAlpha(150);
            p.setPen(QPen(c, 5, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(4, 12), QPointF(12, 4));
        });
    case AnnotationTool::Text:
        return makeIcon([](QPainter& p) {
            QFont f = p.font();
            f.setPixelSize(12);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(0, 0, 16, 16), Qt::AlignCenter, QStringLiteral("T"));
        });
    case AnnotationTool::Badge:
        return makeIcon([](QPainter& p) {
            p.drawEllipse(QRectF(2.5, 2.5, 11, 11));
            QFont f = p.font();
            f.setPixelSize(8);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(2.5, 2.5, 11, 11), Qt::AlignCenter, QStringLiteral("1"));
        });
    case AnnotationTool::Mosaic:
        return makeIcon([](QPainter& p) {
            p.setPen(Qt::NoPen);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    QColor c = kIconColor;
                    c.setAlpha((row + col) % 2 ? 90 : 220);
                    p.fillRect(QRectF(2.5 + col * 4, 2.5 + row * 4, 3.6, 3.6), c);
                }
            }
        });
    case AnnotationTool::Blur:
        return makeIcon([](QPainter& p) {
            p.setPen(Qt::NoPen);
            for (int i = 0; i < 3; ++i) {
                QColor c = kIconColor;
                c.setAlpha(220 - i * 75);
                const qreal r = 2.0 + i * 2.2;
                p.setBrush(c);
                p.drawEllipse(QPointF(8, 8), r, r);
            }
        });
    }
    return {};
}

QIcon widthIcon(int width) {
    return makeIcon([width](QPainter& p) {
        p.setPen(QPen(kIconColor, std::clamp(width * 0.55, 1.2, 4.5), Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(QPointF(3, 8), QPointF(13, 8));
    });
}

QIcon undoIcon() {
    return makeIcon([](QPainter& p) {
        // ↶:经顶部的半弧,左端向下箭头
        p.setPen(QPen(kIconColor, 1.6, Qt::SolidLine, Qt::RoundCap));
        const QRectF r(3.2, 4.6, 9.6, 9.6);
        QPainterPath tail;
        tail.arcMoveTo(r, 180);
        tail.arcTo(r, 180, -180);
        p.drawPath(tail);
        QPainterPath head(QPointF(3.2, 13.0));
        head.lineTo(QPointF(1.2, 9.2));
        head.lineTo(QPointF(5.2, 9.2));
        head.closeSubpath();
        p.fillPath(head, kIconColor);
    });
}

QIcon redoIcon() {
    return makeIcon([](QPainter& p) {
        // ↷:undo 的镜像
        p.setPen(QPen(kIconColor, 1.6, Qt::SolidLine, Qt::RoundCap));
        const QRectF r(3.2, 4.6, 9.6, 9.6);
        QPainterPath tail;
        tail.arcMoveTo(r, 0);
        tail.arcTo(r, 0, 180);
        p.drawPath(tail);
        QPainterPath head(QPointF(12.8, 13.0));
        head.lineTo(QPointF(10.8, 9.2));
        head.lineTo(QPointF(14.8, 9.2));
        head.closeSubpath();
        p.fillPath(head, kIconColor);
    });
}

QIcon ocrIcon() {
    return makeIcon([](QPainter& p) {
        // 四角扫描框
        p.setPen(QPen(kIconColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal c = 3.2;
        const QPointF tl(2, 2), tr(14, 2), bl(2, 14), br(14, 14);
        p.drawPolyline(QPolygonF({tl + QPointF(0, c), tl, tl + QPointF(c, 0)}));
        p.drawPolyline(QPolygonF({tr - QPointF(c, 0), tr, tr + QPointF(0, c)}));
        p.drawPolyline(QPolygonF({bl - QPointF(0, c), bl, bl + QPointF(c, 0)}));
        p.drawPolyline(QPolygonF({br - QPointF(c, 0), br, br - QPointF(0, c)}));
        QFont f = p.font();
        f.setPixelSize(8);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(2, 2, 12, 12), Qt::AlignCenter, QStringLiteral("T"));
    });
}

QIcon translateIcon() {
    return makeIcon([](QPainter& p) {
        QFont f = p.font();
        f.setPixelSize(9);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(0, 0.5, 10, 9), Qt::AlignCenter, QStringLiteral("文"));
        f.setPixelSize(8);
        p.setFont(f);
        p.drawText(QRectF(7.5, 7, 8, 8), Qt::AlignCenter, QStringLiteral("A"));
        QColor c = kIconColor;
        c.setAlpha(140);
        p.setPen(QPen(c, 1.1, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(6.4, 13.2), QPointF(9.8, 3.0));
    });
}

QIcon scrollIcon() {
    return makeIcon([](QPainter& p) {
        // 屏幕 + 下行箭头:上半矩形,下方箭头示意继续向下拼接
        p.drawRect(QRectF(3, 2.5, 10, 6.5));
        p.setPen(QPen(kIconColor, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(8, 10.6), QPointF(8, 12.6));
        QPainterPath head(QPointF(8, 14.8));
        head.lineTo(QPointF(5.9, 11.8));
        head.lineTo(QPointF(10.1, 11.8));
        head.closeSubpath();
        p.fillPath(head, kIconColor);
    });
}

QIcon pinIcon() {
    return makeIcon([](QPainter& p) {
        // 正置图钉:帽 + 颈 + 托盘 + 针
        p.setPen(Qt::NoPen);
        QPainterPath cap;
        cap.addRoundedRect(QRectF(5.2, 2.2, 5.6, 2.6), 1.2, 1.2);
        p.fillPath(cap, kIconColor);
        p.fillRect(QRectF(6.9, 4.6, 2.2, 2.8), kIconColor);
        QPainterPath flange;
        flange.addRoundedRect(QRectF(4.0, 7.4, 8.0, 1.8), 0.9, 0.9);
        p.fillPath(flange, kIconColor);
        p.setPen(QPen(kIconColor, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(8, 9.4), QPointF(8, 13.6));
    });
}

QIcon saveIcon() {
    return makeIcon([](QPainter& p) {
        // 软盘:切角外框 + 快门 + 标签
        p.setPen(QPen(kIconColor, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath body;
        body.moveTo(QPointF(3, 3));
        body.lineTo(QPointF(10.6, 3));
        body.lineTo(QPointF(13, 5.4));
        body.lineTo(QPointF(13, 13));
        body.lineTo(QPointF(3, 13));
        body.closeSubpath();
        p.drawPath(body);
        p.drawRect(QRectF(5.6, 3.2, 4.2, 2.8));
        p.drawRect(QRectF(5.2, 9.0, 5.6, 4.0));
    });
}

QIcon confirmIcon() {
    return makeIcon([](QPainter& p) {
        p.setPen(QPen(theme::accentHover(), 2.0, Qt::SolidLine, Qt::RoundCap,
                      Qt::RoundJoin));
        QPainterPath check(QPointF(3.0, 8.6));
        check.lineTo(QPointF(6.6, 12.0));
        check.lineTo(QPointF(13.2, 4.4));
        p.drawPath(check);
    });
}

QIcon cancelIcon() {
    return makeIcon([](QPainter& p) {
        p.setPen(QPen(theme::danger().lighter(118), 1.8, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(QPointF(4.2, 4.2), QPointF(11.8, 11.8));
        p.drawLine(QPointF(11.8, 4.2), QPointF(4.2, 11.8));
    });
}

} // namespace pixora::icons
