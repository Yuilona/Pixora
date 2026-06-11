#include "platform/windows/WinSystemIntegration.h"

#include <QWindow>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

void WinSystemIntegration::setClickThrough(QWindow* window, bool enabled) {
    if (!window) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    LONG exStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    ::SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);
}

} // namespace pixora
