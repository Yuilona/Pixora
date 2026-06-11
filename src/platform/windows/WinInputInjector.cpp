#include "platform/windows/WinInputInjector.h"

#include "platform/windows/WinCoordinates.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

// 直接向目标点下的窗口投递 WM_MOUSEWHEEL,而非 SendInput——
// 后者依赖光标位置,会把用户鼠标劫持到捕获区中心。
// 个别忽略投递消息的应用以手动模式兜底(产品策略,见 §12)。
bool WinInputInjector::sendScroll(const QPoint& globalLogical, int deltaY) {
    const QPoint phys = wincoord::logicalToPhysical(globalLogical);
    const POINT pt{phys.x(), phys.y()};
    HWND hwnd = ::WindowFromPoint(pt);
    if (!hwnd) {
        return false;
    }
    const WPARAM wParam = MAKEWPARAM(0, static_cast<WORD>(static_cast<SHORT>(deltaY)));
    const LPARAM lParam = MAKELPARAM(pt.x, pt.y);
    return ::PostMessageW(hwnd, WM_MOUSEWHEEL, wParam, lParam) != 0;
}

} // namespace pixora
