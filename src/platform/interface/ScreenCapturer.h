#pragma once

#include <QImage>

class QScreen;

namespace pixora {

// PAL 纯虚接口:屏幕捕获(见 ARCHITECTURE §4.1)。
// 返回物理像素裸图(devicePixelRatio 恒为 1,DPR 由调用方记录)。
class IScreenCapturer {
public:
    virtual ~IScreenCapturer() = default;

    virtual QImage grabScreen(QScreen* screen) = 0;

    // 抓取屏幕局部(长截图逐帧用);region 为该屏局部逻辑坐标。
    virtual QImage grabScreenRegion(QScreen* screen, const QRect& regionLocal) = 0;
};

} // namespace pixora
