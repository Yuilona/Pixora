#pragma once

#include <QPoint>

namespace pixora {

// PAL 纯虚接口:输入注入,自动滚动长截图用(见 ARCHITECTURE §4.1)。
// sendKey(PageDown 备选驱动)随后续切片加入。
class IInputInjector {
public:
    virtual ~IInputInjector() = default;

    // 在全局逻辑坐标处注入垂直滚轮;deltaY 负值向下滚动内容。
    virtual bool sendScroll(const QPoint& globalLogical, int deltaY) = 0;
};

} // namespace pixora
