#include "core/stitch/RowCanvas.h"

#include <algorithm>
#include <cstring>

namespace pixora {

void RowCanvas::reset(int width) {
    width_ = width;
    rows_ = 0;
    chunks_.clear();
}

void RowCanvas::truncate(int rows) {
    rows_ = std::clamp(rows, 0, rows_);
    // 释放截断后完全空出的块(撤销/回刷后不长期占内存)
    const size_t needed = static_cast<size_t>((rows_ + kChunkRows - 1) / kChunkRows);
    if (chunks_.size() > needed + 1) {
        chunks_.resize(needed + 1);
    }
}

void RowCanvas::appendRows(const QImage& src, int srcY, int count) {
    const size_t bytes = static_cast<size_t>(width_) * 4;
    for (int i = 0; i < count; ++i) {
        const int row = rows_ + i;
        const size_t chunk = static_cast<size_t>(row / kChunkRows);
        if (chunk >= chunks_.size()) {
            chunks_.emplace_back(width_, kChunkRows, QImage::Format_ARGB32);
        }
        std::memcpy(chunks_[chunk].scanLine(row % kChunkRows), src.constScanLine(srcY + i),
                    bytes);
    }
    rows_ += count;
}

void RowCanvas::copyRowsInto(int y, int count, QImage& dst, int dstY) const {
    const size_t bytes = static_cast<size_t>(width_) * 4;
    for (int i = 0; i < count; ++i) {
        const int row = y + i;
        std::memcpy(dst.scanLine(dstY + i),
                    chunks_[static_cast<size_t>(row / kChunkRows)].constScanLine(
                        row % kChunkRows),
                    bytes);
    }
}

QImage RowCanvas::copyRows(int y, int count) const {
    y = std::clamp(y, 0, rows_);
    count = std::clamp(count, 0, rows_ - y);
    if (count == 0) {
        return {};
    }
    QImage out(width_, count, QImage::Format_ARGB32);
    copyRowsInto(y, count, out, 0);
    return out;
}

} // namespace pixora
