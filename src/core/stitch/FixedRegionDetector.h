#pragma once

#include <QImage>

namespace pixora {

// 固定区域检测:逐对比较连续帧,滚动中始终逐字节相同的顶部/底部
// 行带即 sticky header/footer(见 ARCHITECTURE §5.3.2)。
// 首对差异帧即生效(否则首次拼接的模板带会混入底栏),估计随帧数
// 单调收紧(取最小连续行数);上限封顶(顶 1/3、底 1/4)防空白页误判。
class FixedRegionDetector {
public:
    // 喂入连续帧(ARGB32);与上一帧相同的帧自动跳过。
    void feed(const QImage& frame);

    int top() const;    // 顶部固定行数(未稳定时为 0)
    int bottom() const; // 底部固定行数(未稳定时为 0)
    int samples() const { return samples_; }

private:
    QImage prev_;
    int topRun_ = 0;
    int bottomRun_ = 0;
    int samples_ = 0;
};

} // namespace pixora
