#pragma once

#include <QPoint>
#include <QRect>
#include <Qt>

class QPainter;
class QColor;

namespace pixora {

// 选区八向手柄:命中测试 / 光标形状 / 缩放计算均为纯函数,可独立单测;
// 绘制由 OverlayWindow 调用。
namespace SelectionHandles {

enum class Hit {
    None,
    Inside,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left,
};

Hit hitTest(const QRect& selection, const QPoint& pos, int margin = 6);

Qt::CursorShape cursorFor(Hit hit);

// 以按下时的选区为基准,按手柄方向应用位移;结果已 normalized。
QRect resized(const QRect& base, Hit hit, const QPoint& delta);

void paint(QPainter& painter, const QRect& selection, const QColor& color);

} // namespace SelectionHandles

} // namespace pixora
