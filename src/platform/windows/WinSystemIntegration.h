#pragma once

#include "platform/interface/SystemIntegration.h"

namespace pixora {

// Windows 实现:点击穿透 = WS_EX_TRANSPARENT(配合 WS_EX_LAYERED);
// 开机自启 = HKCU\...\Run 注册表项(值 = 引号包裹的当前 exe 绝对路径)。
// setAutoStart 写入后回读校验并返回成败;isAutoStartEnabled 路径感知,
// 仅当 Run 项指向"当前 exe"时才算启用(便携版被移动/旧路径残留 → 视为未启用)。
class WinSystemIntegration : public ISystemIntegration {
public:
    void setClickThrough(QWindow* window, bool enabled) override;
    bool setAutoStart(bool enabled) override;
    bool isAutoStartEnabled() const override;
};

} // namespace pixora
