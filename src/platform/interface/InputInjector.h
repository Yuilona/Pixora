#pragma once

#include <QPoint>
#include <Qt>

namespace pixora {

// PAL 纯虚接口:输入注入,自动滚动长截图用(见 ARCHITECTURE §4.1)。
class IInputInjector {
public:
    virtual ~IInputInjector() = default;

    // 在全局逻辑坐标处注入垂直滚轮;deltaY 负值向下滚动内容。
    virtual bool sendScroll(const QPoint& globalLogical, int deltaY) = 0;

    // 向该点下窗口注入按键(PageDown 备选驱动用)。
    virtual bool sendKey(const QPoint& globalLogical, Qt::Key key) = 0;
};

} // namespace pixora
