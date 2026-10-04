#include "ui/overlay/SelectionHandles.h"

#include <QPainter>

#include <array>
#include <cstdlib>

namespace pixora {
namespace SelectionHandles {

namespace {

struct HandlePoint {
    Hit hit;
    QPoint pos;
};

std::array<HandlePoint, 8> handlePoints(const QRect& sel) {
    const QPoint c = sel.center();
    return {{
        {Hit::TopLeft, sel.topLeft()},
        {Hit::Top, {c.x(), sel.top()}},
        {Hit::TopRight, sel.topRight()},
        {Hit::Right, {sel.right(), c.y()}},
        {Hit::BottomRight, sel.bottomRight()},
        {Hit::Bottom, {c.x(), sel.bottom()}},
        {Hit::BottomLeft, sel.bottomLeft()},
        {Hit::Left, {sel.left(), c.y()}},
    }};
}

bool near(int a, int b, int margin) {
    return std::abs(a - b) <= margin;
}

} // namespace

Hit hitTest(const QRect& selection, const QPoint& pos, int margin) {
    if (selection.isEmpty()) {
        return Hit::None;
    }
    for (const HandlePoint& h : handlePoints(selection)) {
        if (near(pos.x(), h.pos.x(), margin) && near(pos.y(), h.pos.y(), margin)) {
            return h.hit;
        }
    }
    const bool inXSpan = pos.x() >= selection.left() && pos.x() <= selection.right();
    const bool inYSpan = pos.y() >= selection.top() && pos.y() <= selection.bottom();
    if (inXSpan && near(pos.y(), selection.top(), margin)) return Hit::Top;
    if (inXSpan && near(pos.y(), selection.bottom(), margin)) return Hit::Bottom;
    if (inYSpan && near(pos.x(), selection.left(), margin)) return Hit::Left;
    if (inYSpan && near(pos.x(), selection.right(), margin)) return Hit::Right;
    if (selection.contains(pos)) {
        return Hit::Inside;
    }
    return Hit::None;
}

Qt::CursorShape cursorFor(Hit hit) {
    switch (hit) {
    case Hit::TopLeft:
    case Hit::BottomRight:
        return Qt::SizeFDiagCursor;
    case Hit::TopRight:
    case Hit::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Hit::Top:
    case Hit::Bottom:
        return Qt::SizeVerCursor;
    case Hit::Left:
    case Hit::Right:
        return Qt::SizeHorCursor;
    case Hit::Inside:
        return Qt::SizeAllCursor;
    case Hit::None:
        break;
    }
    return Qt::CrossCursor;
}

QRect resized(const QRect& base, Hit hit, const QPoint& delta) {
    QRect r = base;
    switch (hit) {
    case Hit::TopLeft:
        r.setTopLeft(r.topLeft() + delta);
        break;
    case Hit::Top:
        r.setTop(r.top() + delta.y());
        break;
    case Hit::TopRight:
        r.setTopRight(r.topRight() + delta);
        break;
    case Hit::Right:
        r.setRight(r.right() + delta.x());
        break;
    case Hit::BottomRight:
        r.setBottomRight(r.bottomRight() + delta);
        break;
    case Hit::Bottom:
        r.setBottom(r.bottom() + delta.y());
        break;
    case Hit::BottomLeft:
        r.setBottomLeft(r.bottomLeft() + delta);
        break;
    case Hit::Left:
        r.setLeft(r.left() + delta.x());
        break;
    case Hit::Inside:
    case Hit::None:
        break;
    }
    return r.normalized();
}

void paint(QPainter& painter, const QRect& selection, const QColor& color) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    for (const HandlePoint& h : handlePoints(selection)) {
        paintKnob(painter, h.pos, color);
    }
    painter.restore();
}

void paintKnob(QPainter& painter, const QPointF& center, const QColor& color) {
    // 白底圆点 + 主题色描边:浅色、深色底上都醒目,又不像实心方块那样压内容
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.5));
    painter.setBrush(Qt::white);
    painter.drawEllipse(center, 4.5, 4.5);
    painter.restore();
}

} // namespace SelectionHandles
} // namespace pixora
