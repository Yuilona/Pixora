#pragma once

#include "platform/shared/QtScreenCapturer.h"

namespace pixora {

// Windows 抓屏:单张截图沿用 QtScreenCapturer(GDI);长截图的连续抓帧走
// DXGI Desktop Duplication——实测 1920×1080 区域 GDI BitBlt 约 22ms/帧
// (2560×1440 桌面,CAPTUREBLT 与否无差别),DXGI 约 3ms,桌面无更新时
// AcquireNextFrame 立即超时返回,空转 tick 近乎零成本。
// DXGI 不可用(远程桌面、旋转屏、HDR 非 8 位格式、驱动不支持)时
// 自动退回 GDI;会话中途失效(分辨率变化、UAC 安全桌面)先重建,
// 重建失败再退回。
class WinScreenCapturer : public QtScreenCapturer {
public:
    std::unique_ptr<IFrameStream> openFrameStream(QScreen* screen,
                                                  const QRect& regionLocal) override;
};

} // namespace pixora
