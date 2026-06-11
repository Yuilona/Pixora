#pragma once

#include "platform/interface/SystemIntegration.h"

namespace pixora {

// Windows 实现:点击穿透 = WS_EX_TRANSPARENT(配合 WS_EX_LAYERED);
// 开机自启 = HKCU\...\Run 注册表项。
class WinSystemIntegration : public ISystemIntegration {
public:
    void setClickThrough(QWindow* window, bool enabled) override;
    void setAutoStart(bool enabled) override;
    bool isAutoStartEnabled() const override;
};

} // namespace pixora
