#include "core/stitch/Stitcher.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace pixora {

namespace {

constexpr qint64 kUndoBudgetBytes = 128LL * 1024 * 1024; // 撤销历史内存预算
constexpr int kMaxUndoSteps = 20;
constexpr double kMinStripStd = 6.0;  // 模板带最低纹理(灰度标准差),低于视为留白
constexpr int kMaxStripsPerBand = 6;  // 每条竖带最多尝试的有纹理模板带数
constexpr int kPeaksPerBand = 3;      // 每带取前几个峰作候选(终裁交给全局校验)
constexpr int kPeakSuppress = 4;      // 峰邻域抑制半径(行),大于峰肩、小于常见周期
constexpr int kProfileColumns = 48;   // 行剖面列块数(全宽,每条竖带 16 块)
constexpr double kAmbiguityMargin = 0.03; // 次优候选与最优分差小于此 → 周期性歧义
constexpr double kSeamRowTolerance = 1.5; // 接缝行"新旧一致"的平均灰度差上限
constexpr int kSeamContext = 2;           // 接缝上下各需几行一致

// 灰度图零拷贝包装为 cv::Mat;调用方保证 QImage 生命周期覆盖使用期
cv::Mat view(const QImage& gray) {
    return cv::Mat(gray.height(), gray.width(), CV_8UC1, const_cast<uchar*>(gray.constBits()),
                   static_cast<size_t>(gray.bytesPerLine()));
}

// 帧统一为 32 位(抓屏得到的 RGB32 与 ARGB32 字节布局一致,无需转换)
QImage normalizeFrame(const QImage& frame) {
    switch (frame.format()) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return frame;
    default:
        return frame.convertToFormat(QImage::Format_ARGB32);
    }
}

// OpenCV SIMD 灰度化,比 QImage::convertToFormat(Grayscale8) 快约 7 倍
QImage toGray(const QImage& frame) {
    QImage gray(frame.size(), QImage::Format_Grayscale8);
    cv::Mat dst(gray.height(), gray.width(), CV_8UC1, gray.bits(),
                static_cast<size_t>(gray.bytesPerLine()));
    const cv::Mat src(frame.height(), frame.width(), CV_8UC4,
                      const_cast<uchar*>(frame.constBits()),
                      static_cast<size_t>(frame.bytesPerLine()));
    cv::cvtColor(src, dst, cv::COLOR_BGRA2GRAY);
    return gray;
}

// 行剖面:每行按列块取均值(只在水平方向降采样,垂直保持全分辨率)。
// 垂直偏移只看行与行的对应,水平细节对定位是冗余的——在剖面上做 NCC
// 比 0.5x 图上的 matchTemplate 快一个数量级以上,且直接得到整行精度;
// 块均值还顺带平滑了 ClearType/分数缩放的光栅化噪声。最终取舍交给
// 全分辨率的重叠区校验
int profileBlock(int searchWidth) {
    return std::max(2, searchWidth / kProfileColumns);
}

QImage profileOf(const QImage& gray, int searchWidth) {
    const int block = profileBlock(searchWidth);
    const int cols = std::max(1, searchWidth / block);
    QImage profile(cols, gray.height(), QImage::Format_Grayscale8);
    cv::Mat dst(profile.height(), cols, CV_8UC1, profile.bits(),
                static_cast<size_t>(profile.bytesPerLine()));
    cv::resize(view(gray)(cv::Rect(0, 0, cols * block, gray.height())), dst, dst.size(), 0, 0,
               cv::INTER_AREA);
    return profile;
}

bool rowsIdentical(const QImage& a, const QImage& b, int y) {
    return std::memcmp(a.constScanLine(y), b.constScanLine(y),
                       static_cast<size_t>(a.width()) * 4) == 0;
}

// 先采样 16 行快速排除,再全量比较
bool framesIdentical(const QImage& a, const QImage& b) {
    if (a.size() != b.size()) {
        return false;
    }
    const int last = a.height() - 1;
    for (int i = 0; i < 16; ++i) {
        if (!rowsIdentical(a, b, last * i / 15)) {
            return false;
        }
    }
    for (int y = 0; y <= last; ++y) {
        if (!rowsIdentical(a, b, y)) {
            return false;
        }
    }
    return true;
}

double ncc(const cv::Mat& a, const cv::Mat& b) {
    cv::Scalar meanA, sdA, meanB, sdB;
    cv::meanStdDev(a, meanA, sdA);
    cv::meanStdDev(b, meanB, sdB);
    const double denom = sdA[0] * sdB[0];
    if (denom < 1e-6) {
        return 0.0; // 平坦区无从校验
    }
    const double n = static_cast<double>(a.rows) * a.cols;
    return (a.dot(b) / n - meanA[0] * meanB[0]) / denom;
}

// 全重叠区校验分:cur 第 y 行 ↔ prev 第 y+dy 行,内容区 [top, bottom)、
// 左起 width 列。每 4 行取 1 行(步长放大的零拷贝视图)省 3/4 计算,
// 采样行仍逐行对应,对整数行偏移(含 ±1)的判别力不受影响。重叠过小返回 -1
double overlapScore(const QImage& prev, const QImage& cur, int dy, int top, int bottom,
                    int width) {
    const int y0 = std::max(top, top - dy);
    const int y1 = std::min(bottom, bottom - dy);
    constexpr int kRowStride = 4;
    const int rows = (y1 - y0 + kRowStride - 1) / kRowStride;
    if (rows < 4) {
        return -1.0;
    }
    const cv::Mat a(rows, width, CV_8UC1, const_cast<uchar*>(cur.constScanLine(y0)),
                    static_cast<size_t>(cur.bytesPerLine()) * kRowStride);
    const cv::Mat b(rows, width, CV_8UC1, const_cast<uchar*>(prev.constScanLine(y0 + dy)),
                    static_cast<size_t>(prev.bytesPerLine()) * kRowStride);
    return ncc(a, b);
}

// 列对齐 NCC(与 TM_CCOEFF_NORMED 等价,要求模板与搜索区同宽):得分图只有
// 一列,只沿 y 滑动。模板与搜索区都是连续存储时,第 y 个分子就是两段
// 连续内存的一次点积。模板先去均值,分子直接是协方差,没有大数相消;
// 窗口方差由逐行和/平方和的整数前缀和精确得到
cv::Mat alignedNcc(const cv::Mat& search, const cv::Mat& templ) {
    const int th = templ.rows;
    const int w = templ.cols;
    const int n = search.rows - th + 1;
    cv::Mat scores(std::max(n, 1), 1, CV_32F, cv::Scalar(0.0f));
    if (n <= 0) {
        return scores;
    }
    cv::Mat t;
    templ.convertTo(t, CV_32F);
    t -= cv::mean(t)[0];
    const double tNorm = cv::norm(t);
    if (tNorm < 1e-6) {
        return scores; // 平坦模板无从匹配
    }
    cv::Mat s;
    search.convertTo(s, CV_32F, 1.0, -128.0); // 居中缩小量级,float 累加更稳
    const cv::Mat tFlat(1, th * w, CV_32F, t.ptr<float>());

    std::vector<qint64> sum(static_cast<size_t>(search.rows) + 1, 0);
    std::vector<qint64> sq(static_cast<size_t>(search.rows) + 1, 0);
    for (int i = 0; i < search.rows; ++i) {
        const uchar* row = search.ptr<uchar>(i);
        qint64 a = 0;
        qint64 b = 0;
        for (int x = 0; x < w; ++x) {
            a += row[x];
            b += row[x] * row[x];
        }
        sum[static_cast<size_t>(i) + 1] = sum[static_cast<size_t>(i)] + a;
        sq[static_cast<size_t>(i) + 1] = sq[static_cast<size_t>(i)] + b;
    }
    const double count = static_cast<double>(th) * w;
    for (int y = 0; y < n; ++y) {
        const double num = cv::Mat(1, th * w, CV_32F, s.ptr<float>(y)).dot(tFlat);
        const auto y0 = static_cast<size_t>(y);
        const auto y1 = static_cast<size_t>(y + th);
        const double ws = static_cast<double>(sum[y1] - sum[y0]);
        const double var = static_cast<double>(sq[y1] - sq[y0]) - ws * ws / count;
        const double denom = std::sqrt(std::max(var, 0.0)) * tNorm;
        scores.at<float>(y) = denom > 1e-6 ? static_cast<float>(num / denom) : 0.0f;
    }
    return scores;
}

// 单列得分图中取前 k 个峰(邻域抑制),低于 floor 的不要
std::vector<int> topPeaks(const cv::Mat& scores, int k, double floor) {
    std::vector<int> peaks;
    cv::Mat work = scores.clone();
    for (int i = 0; i < k; ++i) {
        double maxVal = 0.0;
        cv::Point loc;
        cv::minMaxLoc(work, nullptr, &maxVal, nullptr, &loc);
        if (maxVal < floor) {
            break;
        }
        peaks.push_back(loc.y);
        const int y0 = std::max(0, loc.y - kPeakSuppress);
        const int y1 = std::min(work.rows - 1, loc.y + kPeakSuppress);
        work.rowRange(y0, y1 + 1).setTo(cv::Scalar(-1.0));
    }
    return peaks;
}

// 一条竖带、一个模板带位置 → 候选 dy(追加到 out),返回是否有产出。
// 在行剖面上匹配:模板 = prev 剖面 [stripTop, stripTop+stripH) 行、
// [c0, c0+cw) 列;在 cur 剖面的内容区 [top, bottom) 中搜索
bool bandCandidates(const QImage& prevProfile, const QImage& curProfile, int c0, int cw,
                    int stripTop, int stripH, int top, int bottom, double minScore,
                    std::vector<int>& out) {
    const cv::Mat templ = view(prevProfile)(cv::Rect(c0, stripTop, cw, stripH));
    const cv::Mat search = view(curProfile)(cv::Rect(c0, top, cw, bottom - top));
    const std::vector<int> peaks = topPeaks(alignedNcc(search, templ), kPeaksPerBand, minScore);
    for (const int peak : peaks) {
        out.push_back(stripTop - (top + peak));
    }
    return !peaks.empty();
}

} // namespace

qint64 Stitcher::UndoStep::bytes() const {
    return backup.sizeInBytes() + lastFrame.sizeInBytes() + lastGray.sizeInBytes() +
           lastProfile.sizeInBytes();
}

Stitcher::Stitcher() : Stitcher(Config{}) {}

Stitcher::Stitcher(Config config) : config_(config) {}

void Stitcher::begin(const QImage& firstFrame) {
    const QImage frame = normalizeFrame(firstFrame);
    canvas_.reset(frame.width());
    canvas_.appendRows(frame, 0, frame.height());
    const qint64 byPixels = config_.maxCanvasPixels / std::max(1, frame.width());
    maxRows_ = std::max<int>(frame.height(),
                             static_cast<int>(std::min<qint64>(config_.maxCanvasHeight, byPixels)));
    lastFrame_ = frame;
    lastGray_ = toGray(frame);
    lastProfile_ = profileOf(lastGray_, searchWidth());
    lastContentBottom_ = frame.height();
    segmentStart_ = 0;
    lastDy_ = 0;
    fixedDetector_ = FixedRegionDetector();
    undo_.clear();
}

int Stitcher::searchWidth() const {
    const int w = canvas_.width();
    return std::min(w, std::max(16, w - config_.rightGuard));
}

Stitcher::Match Stitcher::locate(const QImage& gray, const QImage& profile, int top,
                                 int bottom) const {
    const int width = searchWidth();
    const int stripH = std::min(config_.templateStripHeight, (bottom - top) / 3);
    if (stripH < 8) {
        return {};
    }
    const int block = profileBlock(width);
    const int bandCount = width >= 96 && profile.width() >= 24 ? 3 : 1;
    const int bandCols = profile.width() / bandCount;

    // 1. 候选:各带自下而上找有纹理的模板带(下方的带容许更大位移,
    //    优先);留白带跳过不计数,所以大段空白不会挡住上方的正文
    std::vector<int> candidates;
    const int firstStrip = std::max(top, bottom - config_.bottomGuard - stripH);
    for (int band = 0; band < bandCount; ++band) {
        const int c0 = band * bandCols;
        const cv::Rect pixels(c0 * block, 0, bandCols * block, gray.height());
        int tried = 0;
        for (int stripTop = firstStrip; stripTop >= top && tried < kMaxStripsPerBand;
             stripTop -= stripH) {
            cv::Scalar mean, sd;
            cv::meanStdDev(view(lastGray_)(cv::Rect(pixels.x, stripTop, pixels.width, stripH)),
                           mean, sd);
            if (sd[0] < kMinStripStd) {
                continue;
            }
            ++tried;
            if (bandCandidates(lastProfile_, profile, c0, bandCols, stripTop, stripH, top,
                               bottom, config_.minMatchScore, candidates)) {
                break;
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (candidates.empty()) {
        return {};
    }

    // 2. 全局校验:整个重叠区打分择优
    struct Scored {
        int dy;
        double score;
    };
    std::vector<Scored> scored;
    for (const int dy : candidates) {
        const double s = overlapScore(lastGray_, gray, dy, top, bottom, width);
        if (s > -1.0) {
            scored.push_back({dy, s});
        }
    }
    if (scored.empty()) {
        return {};
    }
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });
    Scored best = scored.front();
    // ±1 行精修:窄模板带在分数缩放下偶有一行之差,整区校验分辨得出
    for (const int step : {-1, 1}) {
        const double s = overlapScore(lastGray_, gray, best.dy + step, top, bottom, width);
        if (s > best.score) {
            best = {best.dy + step, s};
        }
    }
    if (best.score < config_.minOverlapScore) {
        spdlog::debug("stitcher: best overlap score {:.3f} (dy {}) below floor", best.score,
                      best.dy);
        return {};
    }
    for (const Scored& other : scored) {
        if (std::abs(other.dy - best.dy) > 2 && other.score >= best.score - kAmbiguityMargin) {
            spdlog::debug("stitcher: ambiguous dy {} ({:.3f}) vs {} ({:.3f})", best.dy,
                          best.score, other.dy, other.score);
            return {};
        }
    }
    return {true, best.dy};
}

// 在重叠区下半段找接缝:接缝上下各 kSeamContext 行新旧一致(平均灰度差
// 极小),取最靠上的——覆盖范围最大。找不到(到处在变)退回 seamMax,
// 即只追加新露出的行(旧版行为)。只看下半段:未识别的顶部悬浮物
// (导航条)不会被刷进画布中段
int Stitcher::chooseSeam(const QImage& gray, int dy, int top, int seamMax) const {
    const int seamMin = top + (seamMax - top) / 2;
    const int width = std::max(16, gray.width() - config_.rightGuard);
    const int first = std::max(top, seamMin - kSeamContext);
    std::vector<char> agree(static_cast<size_t>(std::max(0, seamMax - first)), 0);
    for (int y = first; y < seamMax; ++y) {
        const cv::Mat a(1, width, CV_8UC1, const_cast<uchar*>(gray.constScanLine(y)));
        const cv::Mat b(1, width, CV_8UC1, const_cast<uchar*>(lastGray_.constScanLine(y + dy)));
        agree[static_cast<size_t>(y - first)] =
            cv::norm(a, b, cv::NORM_L1) / width <= kSeamRowTolerance;
    }
    for (int s = seamMin; s < seamMax; ++s) {
        const int lo = std::max(first, s - kSeamContext);
        const int hi = std::min(seamMax - 1, s + kSeamContext - 1);
        bool ok = true;
        for (int y = lo; y <= hi && ok; ++y) {
            ok = agree[static_cast<size_t>(y - first)] != 0;
        }
        if (ok) {
            return s;
        }
    }
    return seamMax;
}

void Stitcher::overwriteFrom(const QImage& frame, int seam, int canvasRow,
                             int newContentBottom) {
    canvas_.truncate(canvasRow);
    canvas_.appendRows(frame, seam, newContentBottom - seam);
}

Stitcher::AppendResult Stitcher::append(const QImage& rawFrame) {
    if (!active() || rawFrame.size() != lastFrame_.size()) {
        spdlog::warn("stitcher: frame size mismatch or not started");
        return AppendResult::MatchFailed;
    }
    const QImage frame = normalizeFrame(rawFrame);
    if (framesIdentical(frame, lastFrame_)) {
        return AppendResult::NoNewContent;
    }

    const int h = frame.height();
    const QImage gray = toGray(frame);
    const QImage profile = profileOf(gray, searchWidth());
    const FixedRegionDetector::Runs raw =
        FixedRegionDetector::measure(lastGray_, gray, config_.rightGuard);

    // 已有滚动样本用采用值;否则先用本对原始测量(首次拼接就得排除底栏)
    const bool sampled = fixedDetector_.samples() > 0;
    const int contentTop = sampled ? fixedDetector_.top() : std::min(raw.top, h / 3);
    const int contentBottom = h - (sampled ? fixedDetector_.bottom() : std::min(raw.bottom, h / 4));
    if (contentBottom - contentTop < 24) {
        return AppendResult::MatchFailed;
    }

    const Match match = locate(gray, profile, contentTop, contentBottom);
    if (!match.found) {
        return AppendResult::MatchFailed;
    }
    const int dy = match.dy;
    if (dy != 0) {
        fixedDetector_.addSample(raw, h); // 只有真滚动的帧对才算固定区证据
    }
    if (dy < 0) {
        return AppendResult::NoNewContent; // 回滚
    }

    // 画布末端 ↔ 上一帧第 lastContentBottom_ 行 ↔ 本帧第 (它 - dy) 行。
    // 上一帧底栏行([contentBottom, h))若已在画布里,接缝上限压到其上方,
    // 截断时一并剔除
    const int seamMax = std::min(lastContentBottom_, contentBottom) - dy;
    if (seamMax <= contentTop) {
        return AppendResult::MatchFailed;
    }
    const int seam = chooseSeam(gray, dy, contentTop, seamMax);
    const int canvasRow = canvas_.rows() - (lastContentBottom_ - dy - seam);
    const int newRows = canvasRow + (contentBottom - seam);

    if (dy == 0) {
        // 没滚动但画面变了(懒加载完成、悬停):原地刷新最后一段,
        // 让成图反映最新状态;不越过最后一段的起点(撤销才能精确还原)
        if (canvasRow >= segmentStart_ && newRows == canvas_.rows()) {
            overwriteFrom(frame, seam, canvasRow, contentBottom);
            lastFrame_ = frame;
            lastGray_ = gray;
            lastProfile_ = profile;
        }
        return AppendResult::NoNewContent;
    }
    // 以成图高度(画布 + 末尾底栏)判增长:首次剔除底栏时画布可能反而变短
    const int newResultHeight = newRows + (h - contentBottom);
    if (newResultHeight > maxRows_) {
        spdlog::warn("stitcher: max height {} reached", maxRows_);
        return AppendResult::LimitReached;
    }
    if (newResultHeight <= resultHeight()) {
        return AppendResult::NoNewContent;
    }

    UndoStep step;
    step.canvasRow = canvasRow;
    step.backup = canvas_.copyRows(canvasRow, canvas_.rows() - canvasRow);
    step.prevRows = canvas_.rows();
    step.prevSegmentStart = segmentStart_;
    step.lastFrame = lastFrame_;
    step.lastGray = lastGray_;
    step.lastProfile = lastProfile_;
    step.lastContentBottom = lastContentBottom_;
    step.lastDy = lastDy_;

    overwriteFrom(frame, seam, canvasRow, contentBottom);
    pushUndo(std::move(step));
    segmentStart_ = canvasRow;
    lastFrame_ = frame;
    lastGray_ = gray;
    lastProfile_ = profile;
    lastContentBottom_ = contentBottom;
    lastDy_ = dy;
    return AppendResult::Appended;
}

void Stitcher::pushUndo(UndoStep step) {
    undo_.push_back(std::move(step));
    qint64 total = 0;
    for (const UndoStep& s : undo_) {
        total += s.bytes();
    }
    while (undo_.size() > 1 &&
           (undo_.size() > kMaxUndoSteps || total > kUndoBudgetBytes)) {
        total -= undo_.front().bytes();
        undo_.pop_front();
    }
}

bool Stitcher::undo() {
    if (undo_.empty()) {
        return false;
    }
    UndoStep step = std::move(undo_.back());
    undo_.pop_back();
    canvas_.truncate(step.canvasRow);
    if (!step.backup.isNull()) {
        canvas_.appendRows(step.backup, 0, step.backup.height());
    }
    segmentStart_ = step.prevSegmentStart;
    lastFrame_ = step.lastFrame;
    lastGray_ = step.lastGray;
    lastProfile_ = step.lastProfile;
    lastContentBottom_ = step.lastContentBottom;
    lastDy_ = step.lastDy;
    return true;
}

int Stitcher::resultHeight() const {
    if (!active()) {
        return 0;
    }
    return canvas_.rows() + (lastFrame_.height() - lastContentBottom_);
}

int Stitcher::contentHeight() const {
    if (!active()) {
        return 0;
    }
    return lastFrame_.height() - fixedDetector_.top() - fixedDetector_.bottom();
}

QImage Stitcher::tail(int maxRows) const {
    if (!active() || maxRows <= 0) {
        return {};
    }
    const int rows = std::min(maxRows, canvas_.rows());
    return canvas_.copyRows(canvas_.rows() - rows, rows);
}

QImage Stitcher::result() const {
    if (!active()) {
        return {};
    }
    // 画布 + 上一帧画布末端以下的行(sticky footer 在成图底部保留一次)
    const int footer = lastFrame_.height() - lastContentBottom_;
    QImage out(canvas_.width(), canvas_.rows() + footer, QImage::Format_ARGB32);
    canvas_.copyRowsInto(0, canvas_.rows(), out, 0);
    const size_t bytes = static_cast<size_t>(out.width()) * 4;
    for (int i = 0; i < footer; ++i) {
        std::memcpy(out.scanLine(canvas_.rows() + i),
                    lastFrame_.constScanLine(lastContentBottom_ + i), bytes);
    }
    return out;
}

} // namespace pixora
