#pragma once

#include <QPoint>
#include <QRect>
#include <QtGlobal>

namespace pixora {

// UI 元素级定位(Windows: UIAutomation):在指定顶层窗口内找
// 光标处最深的可见元素矩形(全局逻辑坐标)。
// 失败/不可用返回空矩形,调用方退回窗口级吸附。
// 注意:实现可能较慢(几十毫秒),调用方负责节流;
// 查询基于窗口句柄 + 几何下钻,不做屏幕命中测试
// (截图遮罩全屏置顶,ElementFromPoint 只会命中遮罩自身)。
class IElementLocator {
public:
    virtual ~IElementLocator() = default;

    virtual QRect elementAt(quintptr windowId, const QPoint& globalLogical) = 0;
};

} // namespace pixora
