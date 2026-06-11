#pragma once

#include <QPoint>
#include <QRect>

namespace pixora {
namespace wincoord {

// Windows 原生坐标(物理像素)↔ Qt 逻辑坐标换算。
// 主屏对主屏,其余按枚举顺序对位——M1 简化策略,M4 跨平台里程碑
// 跑多屏手测矩阵时再换稳健匹配(按设备名)。
QRect physicalToLogical(const QRect& physical);
QPoint logicalToPhysical(const QPoint& logical);

} // namespace wincoord
} // namespace pixora
