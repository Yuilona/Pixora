#pragma once

class QWindow;

namespace pixora {

// PAL 纯虚接口:系统集成杂项(见 ARCHITECTURE §4.1)。
// M2 先落点击穿透;setAutoStart / requestPermissions 随后续里程碑。
class ISystemIntegration {
public:
    virtual ~ISystemIntegration() = default;

    virtual void setClickThrough(QWindow* window, bool enabled) = 0;
};

} // namespace pixora
