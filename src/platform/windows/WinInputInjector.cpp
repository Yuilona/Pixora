#include "platform/windows/WinInputInjector.h"

#include <QCursor>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

bool WinInputInjector::sendScroll(const QPoint& globalLogical, int deltaY) {
    QCursor::setPos(globalLogical); // Qt 处理逻辑→物理换算

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(deltaY);
    return ::SendInput(1, &input, sizeof(INPUT)) == 1;
}

} // namespace pixora
