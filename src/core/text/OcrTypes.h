#pragma once

#include <QRect>
#include <QString>

namespace pixora {

// OCR 识别出的一行文字及其在图像中的像素框。
// box 可能无效(宽高为 0):部分视觉模型只回文字不回位置,
// 提取文字仍可用,翻译回绘则降级处理。
struct OcrLine {
    QString text;
    QRect box; // 图像设备像素坐标
};

} // namespace pixora
