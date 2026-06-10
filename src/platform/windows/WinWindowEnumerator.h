#pragma once

#include "platform/interface/WindowEnumerator.h"

namespace pixora {

// Windows 实现:EnumWindows(天然 Z 序)+ DWMWA_EXTENDED_FRAME_BOUNDS
// 取去阴影的真实边界;过滤不可见/最小化/cloaked/工具窗/本进程窗口。
class WinWindowEnumerator : public IWindowEnumerator {
public:
    std::vector<WindowInfo> topLevelWindows() override;
    std::optional<WindowInfo> windowAt(QPoint globalLogical) override;
};

} // namespace pixora
