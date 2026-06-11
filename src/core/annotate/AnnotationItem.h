#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QPoint>
#include <QPolygon>
#include <QRect>
#include <QString>

#include <algorithm>
#include <utility>

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
    virtual void translate(const QPoint& delta) = 0;

    // 选中命中测试(包围盒外扩 margin);后续可按条目类型精化
    virtual bool hitTest(const QPoint& pos, int margin = 4) const {
        return bounds().adjusted(-margin, -margin, margin, margin).contains(pos);
    }

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
    void translate(const QPoint& delta) override { rect.translate(delta); }

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
    void translate(const QPoint& delta) override {
        from += delta;
        to += delta;
    }

    QPoint from;
    QPoint to;
};

// 画笔 / 马克笔(自由折线)
class PenItem : public AnnotationItem {
public:
    PenItem(AnnotationTool tool, StrokeStyle style, const QPolygon& points)
        : AnnotationItem(tool, style), points(points) {}

    QRect bounds() const override { return points.boundingRect(); }
    void translate(const QPoint& delta) override { points.translate(delta); }

    QPolygon points;
};

class TextItem : public AnnotationItem {
public:
    TextItem(StrokeStyle style, const QPoint& pos, QString text)
        : AnnotationItem(AnnotationTool::Text, style), pos(pos), text(std::move(text)) {}

    // 粗略包围盒(精确测量需字体引擎,属 UI 层);仅用于退化判定
    QRect bounds() const override {
        const int h = textPixelSizeFor(style());
        return QRect(pos, QSize(std::max(1, int(text.size()) * h), h + h / 2));
    }
    void translate(const QPoint& delta) override { pos += delta; }

    QPoint pos; // 文本框左上角
    QString text;
};

class BadgeItem : public AnnotationItem {
public:
    BadgeItem(StrokeStyle style, const QPoint& center, int number)
        : AnnotationItem(AnnotationTool::Badge, style), center(center), number(number) {}

    QRect bounds() const override {
        const int r = badgeRadiusFor(style());
        return QRect(center - QPoint(r, r), QSize(2 * r, 2 * r));
    }
    void translate(const QPoint& delta) override { center += delta; }

    QPoint center;
    int number;
};

} // namespace pixora
