#pragma once

#include <QImage>

#include <vector>

namespace pixora {

// 长截图画布:按固定行数分块存储的 ARGB32 行序列,只支持尾部截断与追加。
// 相比单张预分配画布 2x 扩容:无整画布重分配/拷贝(长图后期每次扩容
// 要搬几百 MB),内存只比实际多出不到一块。
// 源图须为 32 位格式(RGB32/ARGB32,字节布局一致,按行 memcpy)。
class RowCanvas {
public:
    void reset(int width);

    int width() const { return width_; }
    int rows() const { return rows_; }

    void truncate(int rows);
    void appendRows(const QImage& src, int srcY, int count);
    QImage copyRows(int y, int count) const; // 结果为 ARGB32

    // 把 [y, y+count) 行拷进 dst 的 dstY 起始行(dst 同宽 32 位)
    void copyRowsInto(int y, int count, QImage& dst, int dstY) const;

private:
    static constexpr int kChunkRows = 1024;

    int width_ = 0;
    int rows_ = 0;
    std::vector<QImage> chunks_;
};

} // namespace pixora
