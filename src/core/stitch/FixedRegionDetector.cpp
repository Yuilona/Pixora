#include "core/stitch/FixedRegionDetector.h"

#include <algorithm>
#include <cstring>

namespace pixora {

namespace {

bool rowsEqual(const QImage& a, const QImage& b, int y) {
    return std::memcmp(a.constScanLine(y), b.constScanLine(y),
                       static_cast<size_t>(a.width()) * 4) == 0;
}

} // namespace

void FixedRegionDetector::feed(const QImage& frame) {
    if (prev_.isNull()) {
        prev_ = frame;
        return;
    }
    if (frame == prev_ || frame.size() != prev_.size()) {
        return;
    }

    const int h = frame.height();
    int topRun = 0;
    while (topRun < h && rowsEqual(prev_, frame, topRun)) {
        ++topRun;
    }
    int bottomRun = 0;
    while (bottomRun < h - topRun && rowsEqual(prev_, frame, h - 1 - bottomRun)) {
        ++bottomRun;
    }

    if (samples_ == 0) {
        topRun_ = topRun;
        bottomRun_ = bottomRun;
    } else {
        topRun_ = std::min(topRun_, topRun);
        bottomRun_ = std::min(bottomRun_, bottomRun);
    }
    ++samples_;
    prev_ = frame;
}

int FixedRegionDetector::top() const {
    if (samples_ < 1 || prev_.isNull()) {
        return 0;
    }
    return std::min(topRun_, prev_.height() / 3);
}

int FixedRegionDetector::bottom() const {
    if (samples_ < 1 || prev_.isNull()) {
        return 0;
    }
    return std::min(bottomRun_, prev_.height() / 4);
}

} // namespace pixora
