#include "core/stitch/Stitcher.h"

#include <QPainter>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

namespace pixora {

namespace {

// 包装为 cv::Mat 视图(零拷贝);调用方保证 QImage 生命周期覆盖使用期。
cv::Mat grayView(const QImage& gray) {
    return cv::Mat(gray.height(), gray.width(), CV_8UC1,
                   const_cast<uchar*>(gray.bits()), gray.bytesPerLine());
}

struct BandMatch {
    int dy = 0;
    double score = 0.0;
};

} // namespace

Stitcher::Stitcher(Config config) : config_(config) {}

void Stitcher::begin(const QImage& firstFrame) {
    const QImage frame = firstFrame.convertToFormat(QImage::Format_ARGB32);
    canvas_ = QImage(frame.width(), frame.height() * 4, QImage::Format_ARGB32);
    canvas_.fill(Qt::black);
    QPainter(&canvas_).drawImage(0, 0, frame);
    usedHeight_ = frame.height();
    lastFrame_ = frame;
    fixedDetector_ = FixedRegionDetector();
    fixedDetector_.feed(frame);
    footerTrimmed_ = false;
}

Stitcher::AppendResult Stitcher::append(const QImage& rawFrame) {
    if (!active() || rawFrame.size() != lastFrame_.size()) {
        spdlog::warn("stitcher: frame size mismatch or not started");
        return AppendResult::MatchFailed;
    }
    const QImage frame = rawFrame.convertToFormat(QImage::Format_ARGB32);
    if (frame == lastFrame_) {
        return AppendResult::NoNewContent;
    }
    fixedDetector_.feed(frame);
    const int fixedTop = fixedDetector_.top();
    const int fixedBottom = fixedDetector_.bottom();

    const int h = frame.height();
    const int contentBottom = h - fixedBottom;
    const int searchWidth = std::max(16, frame.width() - config_.rightGuard);
    const int stripH =
        std::min(config_.templateStripHeight, (contentBottom - fixedTop) / 3);
    const int stripTop = contentBottom - config_.bottomGuard - stripH;
    if (stripH < 8 || stripTop <= fixedTop) {
        return AppendResult::MatchFailed;
    }

    const QImage lastGray = lastFrame_.convertToFormat(QImage::Format_Grayscale8);
    const QImage frameGray = frame.convertToFormat(QImage::Format_Grayscale8);

    // 三条水平模板带分别匹配;搜索限制在内容区(排除 sticky 头尾)
    const int searchHeight = contentBottom - fixedTop;
    const int bandCount = searchWidth >= 96 ? 3 : 1;
    const int bandWidth = searchWidth / bandCount;
    std::vector<BandMatch> matches;
    for (int i = 0; i < bandCount; ++i) {
        const int bx = i * bandWidth;
        const cv::Mat templ =
            grayView(lastGray)(cv::Rect(bx, stripTop, bandWidth, stripH));
        const cv::Mat search =
            grayView(frameGray)(cv::Rect(bx, fixedTop, bandWidth, searchHeight));
        cv::Mat scores;
        cv::matchTemplate(search, templ, scores, cv::TM_CCOEFF_NORMED);
        double maxScore = 0.0;
        cv::Point maxLoc;
        cv::minMaxLoc(scores, nullptr, &maxScore, nullptr, &maxLoc);
        if (maxScore >= config_.minMatchScore) {
            matches.push_back(BandMatch{stripTop - (fixedTop + maxLoc.y), maxScore});
        }
    }
    if (matches.empty()) {
        spdlog::debug("stitcher: all {} band(s) below threshold", bandCount);
        return AppendResult::MatchFailed;
    }

    // 有效带的偏移取中位数(3 带);2 带取分高者,抵抗单带被动画劫持
    int dy = 0;
    if (matches.size() == 3) {
        std::sort(matches.begin(), matches.end(),
                  [](const BandMatch& a, const BandMatch& b) { return a.dy < b.dy; });
        dy = matches[1].dy;
    } else {
        dy = std::max_element(matches.begin(), matches.end(),
                              [](const BandMatch& a, const BandMatch& b) {
                                  return a.score < b.score;
                              })
                 ->dy;
    }

    if (dy <= 0) {
        return AppendResult::NoNewContent;
    }
    if (usedHeight_ + dy > config_.maxCanvasHeight) {
        spdlog::warn("stitcher: max canvas height {} reached", config_.maxCanvasHeight);
        return AppendResult::NoNewContent;
    }

    // 底栏激活的首次拼接:画布尾部还躺着上一帧的底栏,先剔除
    // (此前的拼接总是以"帧底部"收尾,尾部 fixedBottom 行恰为底栏)
    if (fixedBottom > 0 && !footerTrimmed_) {
        usedHeight_ = std::max(1, usedHeight_ - fixedBottom);
        footerTrimmed_ = true;
    }

    ensureCapacity(usedHeight_ + dy);
    QPainter painter(&canvas_);
    painter.drawImage(QPoint(0, usedHeight_), frame,
                      QRect(0, contentBottom - dy, frame.width(), dy));
    painter.end();
    usedHeight_ += dy;
    lastFrame_ = frame;
    return AppendResult::Appended;
}

void Stitcher::ensureCapacity(int neededHeight) {
    if (canvas_.height() >= neededHeight) {
        return;
    }
    QImage bigger(canvas_.width(), std::max(neededHeight, canvas_.height() * 2),
                  QImage::Format_ARGB32);
    bigger.fill(Qt::black);
    QPainter(&bigger).drawImage(0, 0, canvas_, 0, 0, canvas_.width(), usedHeight_);
    canvas_ = std::move(bigger);
}

QImage Stitcher::result() const {
    if (!active()) {
        return {};
    }
    const int fixedBottom = footerTrimmed_ ? fixedDetector_.bottom() : 0;
    QImage out(canvas_.width(), usedHeight_ + fixedBottom, QImage::Format_ARGB32);
    QPainter painter(&out);
    painter.drawImage(0, 0, canvas_, 0, 0, canvas_.width(), usedHeight_);
    if (fixedBottom > 0) {
        // sticky footer 在成图底部保留一次
        painter.drawImage(QPoint(0, usedHeight_), lastFrame_,
                          QRect(0, lastFrame_.height() - fixedBottom,
                                lastFrame_.width(), fixedBottom));
    }
    return out;
}

} // namespace pixora
