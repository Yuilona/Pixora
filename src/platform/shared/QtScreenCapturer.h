#pragma once

#include "platform/interface/ScreenCapturer.h"

namespace pixora {

// 基于 QScreen::grabWindow 的跨平台抓屏(Windows 下内部走 GDI BitBlt)。
// M1 全平台默认实现;Windows Graphics Capture / ScreenCaptureKit 等
// 原生路径作为后续优化替换(见 ARCHITECTURE §4.2)。
class QtScreenCapturer : public IScreenCapturer {
public:
    QImage grabScreen(QScreen* screen) override;
    QImage grabScreenRegion(QScreen* screen, const QRect& regionLocal) override;
};

} // namespace pixora
