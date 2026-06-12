#include "core/annotate/AnnotationRenderer.h"

#include "core/annotate/AnnotationDocument.h"
#include "core/annotate/AnnotationItem.h"
#include "core/capture/DesktopSnapshot.h"

#include <QFont>
#include <QFontMetrics>
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

QPainterPath polylinePath(const QPolygon& points) {
    QPainterPath path(points.first());
    for (int i = 1; i < points.size(); ++i) {
        path.lineTo(points.at(i));
    }
    return path;
}

void drawMarker(QPainter& painter, const PenItem& marker) {
    if (marker.points.isEmpty()) {
        return;
    }
    QColor color = marker.style().color;
    color.setAlpha(110);
    // 整条折线作为单一路径描边,避免分段叠加导致接头加深
    QPen pen(color, marker.style().width * 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.strokePath(polylinePath(marker.points), pen);
}

void drawText(QPainter& painter, const TextItem& item) {
    if (item.text.isEmpty()) {
        return;
    }
    QFont font = painter.font();
    font.setPixelSize(textPixelSizeFor(item.style()));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(item.style().color);
    const QFontMetrics fm(font);
    painter.drawText(QPoint(item.pos.x(), item.pos.y() + fm.ascent()), item.text);
}

void drawBadge(QPainter& painter, const BadgeItem& badge) {
    const int r = badgeRadiusFor(badge.style());
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(badge.style().color);
    painter.drawEllipse(badge.center, r, r);

    QFont font = painter.font();
    font.setPixelSize(r);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(badge.center - QPoint(r, r), QSize(2 * r, 2 * r)),
                     Qt::AlignCenter, QString::number(badge.number));
}

// 栅格效果:从冻结快照取原始底图,降采样后回放(非破坏性,见 §5.2)。
// 注:采样自原始底图而非已叠加标注的合成图,M2 先取此简化语义。
void drawEffect(QPainter& painter, const ShapeItem& item, const DesktopSnapshot* snapshot) {
    const QRect rect = item.rect.normalized();
    if (rect.width() < 4 || rect.height() < 4) {
        return;
    }
    if (!snapshot) {
        painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
        return;
    }
    const QImage src = snapshot->copyRegionLogical(rect);
    if (src.isNull()) {
        return;
    }
    const int factor = std::max(2, effectStrengthFor(item.style()));
    const QSize small =
        (src.size() / factor).expandedTo(QSize(1, 1)); // src.size() 为物理像素
    const bool mosaic = item.tool() == AnnotationTool::Mosaic;
    const Qt::TransformationMode mode =
        mosaic ? Qt::FastTransformation : Qt::SmoothTransformation;
    QImage out = src.scaled(small, Qt::IgnoreAspectRatio, mode)
                     .scaled(src.size(), Qt::IgnoreAspectRatio, mode);
    out.setDevicePixelRatio(src.devicePixelRatio());
    painter.drawImage(rect, out);
}

} // namespace

void renderItem(QPainter& painter, const AnnotationItem& item,
                const DesktopSnapshot* snapshot) {
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
        } else if (!pts.isEmpty()) {
            painter.strokePath(polylinePath(pts), pen);
        }
        break;
    }
    case AnnotationTool::Marker:
        drawMarker(painter, static_cast<const PenItem&>(item));
        break;
    case AnnotationTool::Text:
        drawText(painter, static_cast<const TextItem&>(item));
        break;
    case AnnotationTool::Badge:
        drawBadge(painter, static_cast<const BadgeItem&>(item));
        break;
    case AnnotationTool::Mosaic:
    case AnnotationTool::Blur:
        drawEffect(painter, static_cast<const ShapeItem&>(item), snapshot);
        break;
    }
    painter.restore();
}

void render(QPainter& painter, const AnnotationDocument& document,
            const DesktopSnapshot* snapshot) {
    for (const auto& item : document.items()) {
        renderItem(painter, *item, snapshot);
    }
}

QImage flatten(QImage base, const AnnotationDocument& document,
               const QPoint& regionTopLeft, const DesktopSnapshot& snapshot) {
    if (document.isEmpty()) {
        return base;
    }
    QPainter painter(&base);
    painter.translate(-regionTopLeft);
    render(painter, document, &snapshot);
    return base;
}

} // namespace AnnotationRenderer
} // namespace pixora
