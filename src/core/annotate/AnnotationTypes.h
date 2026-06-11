#pragma once

#include <QColor>

namespace pixora {

// 矢量类:Rect/Ellipse/Arrow/Pen/Marker/Text/Badge;
// 栅格效果类(对底图采样,非破坏性):Mosaic/Blur(见 ARCHITECTURE §5.2)。
enum class AnnotationTool {
    Rect,
    Ellipse,
    Arrow,
    Pen,
    Marker,
    Text,
    Badge,
    Mosaic,
    Blur,
};

struct StrokeStyle {
    QColor color{0xE5, 0x39, 0x35}; // 默认红
    int width = 4;
};

// 由线宽档位(2/4/8)派生各工具的尺寸参数,保持工具条只有一个粗细控制。
inline int textPixelSizeFor(const StrokeStyle& s) {
    return s.width <= 2 ? 14 : (s.width <= 4 ? 20 : 28);
}
inline int badgeRadiusFor(const StrokeStyle& s) {
    return 10 + s.width;
}
inline int effectStrengthFor(const StrokeStyle& s) {
    return s.width * 3; // 马赛克块边长 / 模糊降采样因子(逻辑像素)
}

} // namespace pixora
