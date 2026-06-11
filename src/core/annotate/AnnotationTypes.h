#pragma once

#include <QColor>

namespace pixora {

// M2 第一批矢量工具;文字/序号/马赛克/模糊等随后续切片加入
// (见 ARCHITECTURE §5.2)。
enum class AnnotationTool {
    Rect,
    Ellipse,
    Arrow,
    Pen,
};

struct StrokeStyle {
    QColor color{0xE5, 0x39, 0x35}; // 默认红
    int width = 4;
};

} // namespace pixora
