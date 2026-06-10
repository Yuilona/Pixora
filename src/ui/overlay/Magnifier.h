#pragma once

#include <QImage>
#include <QPoint>
#include <QSize>

class QPainter;

namespace pixora {

// 像素级放大镜:从冻结快照取鼠标周边物理像素,邻近插值放大,
// 叠加十字线 + 逻辑坐标 + RGB 色值(见 ARCHITECTURE §5.5)。
// 遮罩窗绘制阶段调用;独立取色模式(M5)复用本组件。
namespace Magnifier {

struct Context {
    const QImage* physicalImage = nullptr; // DPR=1 裸图
    qreal dpr = 1.0;
    QPoint cursorLocalLogical;  // 所在遮罩窗局部坐标
    QPoint cursorGlobalLogical; // 显示用全局坐标
    QSize widgetSize;
};

void draw(QPainter& painter, const Context& ctx);

} // namespace Magnifier

} // namespace pixora
