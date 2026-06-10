#pragma once

#include <QImage>

class QScreen;

namespace pixora {

// PAL 纯虚接口:屏幕捕获(见 ARCHITECTURE §4.1)。
// 返回该屏幕的物理像素裸图(devicePixelRatio 恒为 1,DPR 由调用方记录)。
class IScreenCapturer {
public:
    virtual ~IScreenCapturer() = default;

    virtual QImage grabScreen(QScreen* screen) = 0;
};

} // namespace pixora
