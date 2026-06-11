#include "platform/windows/WinCoordinates.h"

#include <QGuiApplication>
#include <QScreen>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <vector>

namespace pixora {
namespace wincoord {

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

} // namespace

QRect physicalToLogical(const QRect& physical) {
    const std::vector<MonitorRegion> regions = buildMonitorRegions();
    if (regions.empty()) {
        return physical;
    }
    // 以矩形中心所在屏的换算关系换算整个矩形(跨屏窗口取主要所在屏)
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

QPoint logicalToPhysical(const QPoint& logical) {
    const std::vector<MonitorRegion> regions = buildMonitorRegions();
    if (regions.empty()) {
        return logical;
    }
    const MonitorRegion* hit = &regions.front();
    for (const MonitorRegion& r : regions) {
        if (r.logical.contains(logical)) {
            hit = &r;
            break;
        }
    }
    const QPointF offset = QPointF(logical - hit->logical.topLeft()) * hit->dpr;
    return hit->physical.topLeft() + offset.toPoint();
}

} // namespace wincoord
} // namespace pixora
