#pragma once

#include <QImage>
#include <QPoint>

class QPainter;

namespace pixora {

class AnnotationDocument;
class AnnotationItem;

// 标注渲染:把矢量条目画到 QPainter(遮罩窗实时预览)或
// 平铺到底图(导出),模型不含绘制代码(见 ARCHITECTURE §5.2)。
// 条目几何为全局逻辑坐标;调用方负责 translate 到目标坐标系。
namespace AnnotationRenderer {

void renderItem(QPainter& painter, const AnnotationItem& item);
void render(QPainter& painter, const AnnotationDocument& document);

// 导出:把标注画进底图副本。base 需已设 DPR;regionTopLeft 为
// 底图左上角对应的全局逻辑坐标。
QImage flatten(QImage base, const AnnotationDocument& document,
               const QPoint& regionTopLeft);

} // namespace AnnotationRenderer

} // namespace pixora
