#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QPoint>
#include <QPolygon>
#include <QRect>

#include <algorithm>

namespace pixora {

// 标注条目:纯矢量数据(几何 + 样式),不含任何绘制代码;
// 渲染由 UI 层 AnnotationRenderer 完成(见 ARCHITECTURE §5.2)。
// 几何一律使用全局逻辑坐标。
class AnnotationItem {
public:
    AnnotationItem(AnnotationTool tool, StrokeStyle style) : tool_(tool), style_(style) {}
    virtual ~AnnotationItem() = default;

    AnnotationTool tool() const { return tool_; }
    const StrokeStyle& style() const { return style_; }

    virtual QRect bounds() const = 0;

private:
    AnnotationTool tool_;
    StrokeStyle style_;
};

// 矩形 / 椭圆
class ShapeItem : public AnnotationItem {
public:
    ShapeItem(AnnotationTool tool, StrokeStyle style, const QRect& rect)
        : AnnotationItem(tool, style), rect(rect) {}

    QRect bounds() const override { return rect.normalized(); }

    QRect rect;
};

class ArrowItem : public AnnotationItem {
public:
    ArrowItem(StrokeStyle style, const QPoint& from, const QPoint& to)
        : AnnotationItem(AnnotationTool::Arrow, style), from(from), to(to) {}

    QRect bounds() const override {
        // 两端点都含在内(QRect::normalized 在 Qt6 保持尺寸,会偏移 1px)
        return QRect(QPoint(std::min(from.x(), to.x()), std::min(from.y(), to.y())),
                     QPoint(std::max(from.x(), to.x()), std::max(from.y(), to.y())));
    }

    QPoint from;
    QPoint to;
};

class PenItem : public AnnotationItem {
public:
    PenItem(StrokeStyle style, const QPolygon& points)
        : AnnotationItem(AnnotationTool::Pen, style), points(points) {}

    QRect bounds() const override { return points.boundingRect(); }

    QPolygon points;
};

} // namespace pixora
