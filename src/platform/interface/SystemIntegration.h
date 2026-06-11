#pragma once

class QWindow;

namespace pixora {

// PAL 纯虚接口:系统集成杂项(见 ARCHITECTURE §4.1)。
// requestPermissions(macOS TCC 引导)随 M4。
class ISystemIntegration {
public:
    virtual ~ISystemIntegration() = default;

    virtual void setClickThrough(QWindow* window, bool enabled) = 0;
    virtual void setAutoStart(bool enabled) = 0;
    virtual bool isAutoStartEnabled() const = 0;
};

} // namespace pixora
