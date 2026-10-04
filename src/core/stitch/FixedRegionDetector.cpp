#include "core/stitch/FixedRegionDetector.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace pixora {

namespace {

constexpr int kPixelTolerance = 24; // 单像素灰度差超过此值才算"变了"

// 两行(灰度)是否原地相同:逐字节相同走快路径;否则允许极少量像素
// 明显变化(闪烁光标约 2px 宽、进度条每帧只长几 px)
bool rowsMatch(const QImage& a, const QImage& b, int y, int width) {
    const uchar* pa = a.constScanLine(y);
    const uchar* pb = b.constScanLine(y);
    if (std::memcmp(pa, pb, static_cast<size_t>(width)) == 0) {
        return true;
    }
    const int allowed = std::max(2, width / 200);
    int changed = 0;
    for (int x = 0; x < width; ++x) {
        if (std::abs(int(pa[x]) - int(pb[x])) > kPixelTolerance && ++changed > allowed) {
            return false;
        }
    }
    return true;
}

} // namespace

FixedRegionDetector::Runs FixedRegionDetector::measure(const QImage& prevGray,
                                                       const QImage& curGray,
                                                       int ignoreRight) {
    Runs runs;
    if (prevGray.size() != curGray.size() || prevGray.isNull()) {
        return runs;
    }
    const int h = curGray.height();
    const int width = std::max(1, curGray.width() - std::max(0, ignoreRight));
    while (runs.top < h && rowsMatch(prevGray, curGray, runs.top, width)) {
        ++runs.top;
    }
    while (runs.bottom < h - runs.top &&
           rowsMatch(prevGray, curGray, h - 1 - runs.bottom, width)) {
        ++runs.bottom;
    }
    return runs;
}

void FixedRegionDetector::addSample(Runs raw, int frameHeight) {
    recent_[static_cast<size_t>(samples_ % 3)] = raw;
    ++samples_;
    const int n = std::min(samples_, 3);
    auto pick = [&](auto field) {
        int values[3];
        for (int i = 0; i < n; ++i) {
            values[i] = recent_[static_cast<size_t>(i)].*field;
        }
        std::sort(values, values + n);
        return n == 3 ? values[1] : values[0];
    };
    top_ = std::min(pick(&Runs::top), frameHeight / 3);
    bottom_ = std::min(pick(&Runs::bottom), frameHeight / 4);
}

} // namespace pixora
