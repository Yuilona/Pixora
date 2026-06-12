#pragma once

#include "core/text/OcrTypes.h"

#include <QImage>
#include <QList>
#include <QStringList>

namespace pixora {

// 截图翻译的"无感替换"绘制:每行原文区域用周边采样的背景色抹平,
// 译文按行框回绘(字号自适应,颜色取区内与背景反差最大的色)。
// translations 与 lines 按下标对应;译文为空或行框无效的行原样保留。
QImage renderReplacedText(QImage base, const QList<OcrLine>& lines,
                          const QStringList& translations);

} // namespace pixora
