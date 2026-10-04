#pragma once

#include <QImage>
#include <QRect>

#include <memory>

class QScreen;

namespace pixora {

// 连续抓帧会话(长截图逐帧用):同一区域高频重复抓取。
// 平台可用更快的通道实现(Windows: DXGI Desktop Duplication),
// 并顺带回答"自上次以来桌面是否更新过"——空转 tick 几乎零成本。
class IFrameStream {
public:
    virtual ~IFrameStream() = default;

    // 抓取最新一帧(物理像素,32 位)。*changed 为 false 表示桌面自上次
    // 抓取以来没有更新,此时返回值可为空图,调用方应沿用上一帧。
    virtual QImage grab(bool* changed) = 0;

    // 是否走的是低成本通道(空转检测近乎免费,无需降帧省电)
    virtual bool accelerated() const { return false; }
};

// PAL 纯虚接口:屏幕捕获(见 ARCHITECTURE §4.1)。
// 返回物理像素裸图(devicePixelRatio 恒为 1,DPR 由调用方记录)。
class IScreenCapturer {
public:
    virtual ~IScreenCapturer() = default;

    virtual QImage grabScreen(QScreen* screen) = 0;

    // 抓取屏幕局部;region 为该屏局部逻辑坐标。
    virtual QImage grabScreenRegion(QScreen* screen, const QRect& regionLocal) = 0;

    // 打开连续抓帧会话。默认实现每次调 grabScreenRegion(恒报告有变化)。
    virtual std::unique_ptr<IFrameStream> openFrameStream(QScreen* screen,
                                                          const QRect& regionLocal);
};

// 默认连续抓帧:逐次整区抓取
class RegionGrabStream : public IFrameStream {
public:
    RegionGrabStream(IScreenCapturer& capturer, QScreen* screen, const QRect& regionLocal)
        : capturer_(capturer), screen_(screen), region_(regionLocal) {}

    QImage grab(bool* changed) override {
        *changed = true;
        return capturer_.grabScreenRegion(screen_, region_);
    }

private:
    IScreenCapturer& capturer_;
    QScreen* screen_;
    QRect region_;
};

inline std::unique_ptr<IFrameStream> IScreenCapturer::openFrameStream(QScreen* screen,
                                                                      const QRect& regionLocal) {
    return std::make_unique<RegionGrabStream>(*this, screen, regionLocal);
}

} // namespace pixora
