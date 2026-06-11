#include "core/stitch/Stitcher.h"

#include <QPainter>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace pixora {

namespace {

// 包装为 cv::Mat 视图(零拷贝);调用方保证 QImage 生命周期覆盖使用期。
cv::Mat grayView(const QImage& gray) {
    return cv::Mat(gray.height(), gray.width(), CV_8UC1,
                   const_cast<uchar*>(gray.bits()), gray.bytesPerLine());
}

} // namespace

Stitcher::Stitcher(Config config) : config_(config) {}

void Stitcher::begin(const QImage& firstFrame) {
    const QImage frame = firstFrame.convertToFormat(QImage::Format_ARGB32);
    canvas_ = QImage(frame.width(), frame.height() * 4, QImage::Format_ARGB32);
    canvas_.fill(Qt::black);
    QPainter(&canvas_).drawImage(0, 0, frame);
    usedHeight_ = frame.height();
    lastFrame_ = frame;
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

    const int h = frame.height();
    const int searchWidth = std::max(16, frame.width() - config_.rightGuard);
    const int stripH = std::min(config_.templateStripHeight, h / 3);
    const int stripTop = h - config_.bottomGuard - stripH;
    if (stripTop <= 0) {
        return AppendResult::MatchFailed;
    }

    // 模板带取自上一帧底部(去保护带),在新帧全高范围内搜索
    const QImage lastGray = lastFrame_.convertToFormat(QImage::Format_Grayscale8);
    const QImage frameGray = frame.convertToFormat(QImage::Format_Grayscale8);
    const cv::Mat templ =
        grayView(lastGray)(cv::Rect(0, stripTop, searchWidth, stripH));
    const cv::Mat search = grayView(frameGray)(cv::Rect(0, 0, searchWidth, h));

    cv::Mat scores;
    cv::matchTemplate(search, templ, scores, cv::TM_CCOEFF_NORMED);
    double maxScore = 0.0;
    cv::Point maxLoc;
    cv::minMaxLoc(scores, nullptr, &maxScore, nullptr, &maxLoc);

    if (maxScore < config_.minMatchScore) {
        spdlog::debug("stitcher: match failed, best score {:.3f}", maxScore);
        return AppendResult::MatchFailed;
    }

    const int dy = stripTop - maxLoc.y; // 内容上移量 = 新增行数
    if (dy <= 0) {
        return AppendResult::NoNewContent;
    }
    if (usedHeight_ + dy > config_.maxCanvasHeight) {
        spdlog::warn("stitcher: max canvas height {} reached", config_.maxCanvasHeight);
        return AppendResult::NoNewContent;
    }

    ensureCapacity(usedHeight_ + dy);
    QPainter painter(&canvas_);
    painter.drawImage(QPoint(0, usedHeight_), frame, QRect(0, h - dy, frame.width(), dy));
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
    return canvas_.copy(0, 0, canvas_.width(), usedHeight_);
}

} // namespace pixora
