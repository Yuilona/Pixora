#pragma once

#include "platform/interface/PlatformTypes.h"

#include <QPoint>

#include <optional>
#include <vector>

namespace pixora {

// PAL 纯虚接口:顶层窗口枚举,选区自动吸附用(见 ARCHITECTURE §4.1)。
// elementAt(UI 控件级检测, UIA/AX/AT-SPI)随 M5 加入。
class IWindowEnumerator {
public:
    virtual ~IWindowEnumerator() = default;

    // 按 Z 序(最顶在前)返回可见顶层窗口。
    virtual std::vector<WindowInfo> topLevelWindows() = 0;

    virtual std::optional<WindowInfo> windowAt(QPoint globalLogical) = 0;
};

} // namespace pixora
