#pragma once

#include <QRect>
#include <QString>

#include <cstdint>

namespace pixora {

// 顶层窗口信息(逻辑坐标)。id 为平台原生句柄值(Windows: HWND)。
struct WindowInfo {
    quintptr id = 0;
    QString title;
    QRect frameLogical; // 真实可视边界(Windows 下已去投影阴影)
};

} // namespace pixora
