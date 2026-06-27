#pragma once

class QWindow;

namespace pixora {

// PAL 纯虚接口:系统集成杂项(见 ARCHITECTURE §4.1)。
// requestPermissions(macOS TCC 引导)随 M4。
class ISystemIntegration {
public:
    virtual ~ISystemIntegration() = default;

    virtual void setClickThrough(QWindow* window, bool enabled) = 0;
    // 返回是否确实落地为期望状态:写入后 sync+回读校验,被安全软件/权限挡下时返回 false。
    virtual bool setAutoStart(bool enabled) = 0;
    // 仅当 Run 项存在、指向"当前可执行文件"时才算已启用(路径感知,非仅存在性)。
    virtual bool isAutoStartEnabled() const = 0;
};

} // namespace pixora
