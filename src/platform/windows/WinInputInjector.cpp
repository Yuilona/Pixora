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

bool WinInputInjector::sendKey(const QPoint& globalLogical, Qt::Key key) {
    UINT vk = 0;
    switch (key) {
    case Qt::Key_PageDown:
        vk = VK_NEXT;
        break;
    case Qt::Key_PageUp:
        vk = VK_PRIOR;
        break;
    case Qt::Key_End:
        vk = VK_END;
        break;
    default:
        return false;
    }
    const QPoint phys = wincoord::logicalToPhysical(globalLogical);
    HWND hwnd = ::WindowFromPoint(POINT{phys.x(), phys.y()});
    if (!hwnd) {
        return false;
    }
    ::PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    ::PostMessageW(hwnd, WM_KEYUP, vk, LPARAM(1) << 30 | LPARAM(1) << 31);
    return true;
}

} // namespace pixora
