#pragma once

#include "platform/interface/SystemIntegration.h"

namespace pixora {

// Windows 实现:点击穿透 = WS_EX_TRANSPARENT(配合 WS_EX_LAYERED);
// 开机自启 = HKCU\...\Run 注册表项(值 = "当前 exe 绝对路径" --autostart),
// 生效与否还受 Explorer\StartupApproved\Run 同名项(任务管理器开关)约束。
// setAutoStart 写入后回读校验并返回成败;autoStartStatus 路径感知,
// 区分"指向当前 exe / 旧路径残留 / 另一个仍存在的副本 / 被系统禁用"。
class WinSystemIntegration : public ISystemIntegration {
public:
    void setClickThrough(QWindow* window, bool enabled) override;
    bool setAutoStart(bool enabled) override;
    AutoStartStatus autoStartStatus() const override;
};

} // namespace pixora
