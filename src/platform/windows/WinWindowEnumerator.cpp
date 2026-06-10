#include "platform/windows/WinWindowEnumerator.h"

#include <QGuiApplication>
#include <QScreen>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dwmapi.h>

namespace pixora {

namespace {

// 一块物理屏幕区域与其 Qt 逻辑几何的对应关系。
struct MonitorRegion {
    QRect physical;
    QRect logical;
    qreal dpr = 1.0;
};

struct NativeMonitor {
    RECT rc;
    bool primary;
};

// Windows 原生坐标(物理像素)↔ Qt 逻辑坐标的换算表。
// 主屏对主屏,其余按枚举顺序对位——M1 简化策略,M4 跨平台里程碑
// 跑多屏手测矩阵时再换稳健匹配(按设备名)。
std::vector<MonitorRegion> buildMonitorRegions() {
    std::vector<NativeMonitor> natives;
    ::EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM lp) -> BOOL {
            MONITORINFO info{};
            info.cbSize = sizeof(MONITORINFO);
            if (::GetMonitorInfoW(monitor, &info)) {
                reinterpret_cast<std::vector<NativeMonitor>*>(lp)->push_back(
                    {info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0});
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&natives));

    // 两边都排成"主屏在前,其余保持枚举顺序"
    std::stable_sort(natives.begin(), natives.end(),
                     [](const NativeMonitor& a, const NativeMonitor& b) {
                         return a.primary && !b.primary;
                     });
    QList<QScreen*> screens = QGuiApplication::screens();
    QScreen* primary = QGuiApplication::primaryScreen();
    std::stable_sort(screens.begin(), screens.end(),
                     [primary](QScreen* a, QScreen* b) { return a == primary && b != primary; });

    std::vector<MonitorRegion> regions;
    const size_t count = std::min(natives.size(), static_cast<size_t>(screens.size()));
    for (size_t i = 0; i < count; ++i) {
        const RECT& rc = natives[i].rc;
        regions.push_back(MonitorRegion{
            QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top),
            screens[static_cast<int>(i)]->geometry(),
            screens[static_cast<int>(i)]->devicePixelRatio()});
    }
    return regions;
}

QRect physicalToLogical(const QRect& physical, const std::vector<MonitorRegion>& regions) {
    if (regions.empty()) {
        return physical;
    }
    // 以窗口中心所在屏的换算关系换算整个矩形(跨屏窗口取主要所在屏)
    const QPoint center = physical.center();
    const MonitorRegion* hit = &regions.front();
    for (const MonitorRegion& r : regions) {
        if (r.physical.contains(center)) {
            hit = &r;
            break;
        }
    }
    const QPointF offset = (physical.topLeft() - hit->physical.topLeft()) / hit->dpr;
    return QRect(hit->logical.topLeft() + offset.toPoint(),
                 QSize(qRound(physical.width() / hit->dpr),
                       qRound(physical.height() / hit->dpr)));
}

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

    const std::vector<MonitorRegion> regions = buildMonitorRegions();
    QRect virtualLogical;
    for (const MonitorRegion& r : regions) {
        virtualLogical |= r.logical;
    }

    std::vector<WindowInfo> windows;
    for (HWND hwnd : handles) {
        const QRect physical = windowFrame(hwnd);
        if (physical.isEmpty()) {
            continue;
        }
        const QRect logical = physicalToLogical(physical, regions) & virtualLogical;
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
