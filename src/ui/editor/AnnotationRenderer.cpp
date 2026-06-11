#include "ui/editor/AnnotationRenderer.h"

#include "core/annotate/AnnotationDocument.h"
#include "core/annotate/AnnotationItem.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace pixora {
namespace AnnotationRenderer {

namespace {

void drawArrow(QPainter& painter, const ArrowItem& arrow) {
    const QPointF from(arrow.from);
    const QPointF to(arrow.to);
    const QPointF diff = to - from;
    const double length = std::hypot(diff.x(), diff.y());
    if (length < 1.0) {
        return;
    }
    const double angle = std::atan2(diff.y(), diff.x());
    const double head = std::clamp(8.0 + arrow.style().width * 2.0, 12.0, 28.0);
    constexpr double kSpread = 0.45; // 箭头张角(弧度)

    const QPointF wing1 = to - QPointF(std::cos(angle - kSpread), std::sin(angle - kSpread)) * head;
    const QPointF wing2 = to - QPointF(std::cos(angle + kSpread), std::sin(angle + kSpread)) * head;
    // 杆只画到箭头根部,避免穿出尖端
    const QPointF shaftEnd = to - QPointF(std::cos(angle), std::sin(angle)) * (head * 0.6);

    painter.drawLine(from, shaftEnd);
    QPainterPath headPath(to);
    headPath.lineTo(wing1);
    headPath.lineTo(wing2);
    headPath.closeSubpath();
    painter.fillPath(headPath, arrow.style().color);
}

} // namespace

void renderItem(QPainter& painter, const AnnotationItem& item) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(item.style().color, item.style().width, Qt::SolidLine, Qt::RoundCap,
             Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (item.tool()) {
    case AnnotationTool::Rect:
        painter.drawRect(static_cast<const ShapeItem&>(item).rect.normalized());
        break;
    case AnnotationTool::Ellipse:
        painter.drawEllipse(static_cast<const ShapeItem&>(item).rect.normalized());
        break;
    case AnnotationTool::Arrow:
        drawArrow(painter, static_cast<const ArrowItem&>(item));
        break;
    case AnnotationTool::Pen: {
        const QPolygon& pts = static_cast<const PenItem&>(item).points;
        if (pts.size() == 1) {
            painter.drawPoint(pts.first());
        } else {
            painter.drawPolyline(pts);
        }
        break;
    }
    }
    painter.restore();
}

void render(QPainter& painter, const AnnotationDocument& document) {
    for (const auto& item : document.items()) {
        renderItem(painter, *item);
    }
}

QImage flatten(QImage base, const AnnotationDocument& document,
               const QPoint& regionTopLeft) {
    if (document.isEmpty()) {
        return base;
    }
    QPainter painter(&base);
    painter.translate(-regionTopLeft);
    render(painter, document);
    return base;
}

} // namespace AnnotationRenderer
} // namespace pixora
