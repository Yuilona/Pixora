#include "platform/windows/WinWindowEnumerator.h"

#include "platform/windows/WinCoordinates.h"

#include <QGuiApplication>
#include <QScreen>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dwmapi.h>

namespace pixora {

namespace {

QRect windowFrame(HWND hwnd) {
    RECT rc{};
    if (FAILED(::DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc,
                                       sizeof(rc)))) {
        if (!::GetWindowRect(hwnd, &rc)) {
            return {};
        }
    }
    return QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}

bool isCandidate(HWND hwnd) {
    if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd)) {
        return false;
    }
    const LONG exStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }
    // UWP 挂起的"幽灵窗"是 cloaked 状态
    BOOL cloaked = FALSE;
    if (SUCCEEDED(::DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked,
                                          sizeof(cloaked))) &&
        cloaked) {
        return false;
    }
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId()) {
        return false; // 自家窗口(遮罩等)不参与吸附
    }
    return true;
}

QString windowTitle(HWND hwnd) {
    wchar_t buffer[256];
    const int len = ::GetWindowTextW(hwnd, buffer, 256);
    return QString::fromWCharArray(buffer, len);
}

} // namespace

std::vector<WindowInfo> WinWindowEnumerator::topLevelWindows() {
    std::vector<HWND> handles;
    ::EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            if (isCandidate(hwnd)) {
                reinterpret_cast<std::vector<HWND>*>(lp)->push_back(hwnd);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&handles));

    QRect virtualLogical;
    for (const QScreen* screen : QGuiApplication::screens()) {
        virtualLogical |= screen->geometry();
    }

    std::vector<WindowInfo> windows;
    for (HWND hwnd : handles) {
        const QRect physical = windowFrame(hwnd);
        if (physical.isEmpty()) {
            continue;
        }
        const QRect logical = wincoord::physicalToLogical(physical) & virtualLogical;
        if (logical.isEmpty()) {
            continue;
        }
        windows.push_back(WindowInfo{reinterpret_cast<quintptr>(hwnd),
                                     windowTitle(hwnd), logical});
    }
    return windows;
}

std::optional<WindowInfo> WinWindowEnumerator::windowAt(QPoint globalLogical) {
    for (const WindowInfo& w : topLevelWindows()) {
        if (w.frameLogical.contains(globalLogical)) {
            return w;
        }
    }
    return std::nullopt;
}

} // namespace pixora
