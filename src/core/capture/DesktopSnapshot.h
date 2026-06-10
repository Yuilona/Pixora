#pragma once

#include <QImage>
#include <QRect>

#include <vector>

namespace pixora {

// 单个屏幕的冻结帧:物理像素裸图 + 逻辑几何 + DPR。
struct ScreenSnap {
    QImage image;          // 物理像素(devicePixelRatio 恒为 1)
    QRect geometryLogical; // 虚拟桌面逻辑坐标系下的屏幕几何
    qreal dpr = 1.0;
};

// 虚拟桌面冻结快照。"逻辑坐标 ↔ 各屏物理像素"的换算只允许发生在
// 本类内部,这是混合 DPI 问题的集中防控点(见 ARCHITECTURE §5.1)。
class DesktopSnapshot {
public:
    DesktopSnapshot() = default;
    explicit DesktopSnapshot(std::vector<ScreenSnap> screens);

    const std::vector<ScreenSnap>& screens() const { return screens_; }
    bool isEmpty() const { return screens_.empty(); }
    QRect virtualGeometryLogical() const;

    // 截取逻辑区域,跨屏自动拼合;输出图 DPR 取所涉屏幕的最大值。
    QImage copyRegionLogical(const QRect& region) const;

private:
    std::vector<ScreenSnap> screens_;
};

} // namespace pixora
