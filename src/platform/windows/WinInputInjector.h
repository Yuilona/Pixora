#pragma once

#include "platform/interface/InputInjector.h"

namespace pixora {

// Windows 实现:光标移到目标点后 SendInput 滚轮事件
// (Win10+ 默认滚动光标下窗口,无需切换焦点)。
class WinInputInjector : public IInputInjector {
public:
    bool sendScroll(const QPoint& globalLogical, int deltaY) override;
};

} // namespace pixora
