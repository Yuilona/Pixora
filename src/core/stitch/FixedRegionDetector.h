#pragma once

#include <QImage>

#include <array>

namespace pixora {

// 固定区域检测(见 ARCHITECTURE §5.3.2):滚动中原地不动的顶部/底部行带
// 即 sticky header/footer。
//
// - measure():一对相邻帧(灰度)从上下两端逐行比较,得出"原地相同"的
//   行数。比较排除右侧 ignoreRight 列(滚动条滑块会划过底栏行),并容忍
//   极少量像素差异(输入框光标闪烁、阅读进度条增长)。
// - addSample():只喂"确实发生了滚动"的帧对(纯悬停变化不算证据)。
//   采用值取最近 3 个样本的中位数:单个噪声样本(偶发遮挡)不会像旧版
//   取最小值那样把估计永久打掉。不足 3 个样本时取最小值(保守)。
// - 上限封顶(顶 1/3、底 1/4)防空白页误判。
//
// 估计值随时间变化是安全的:Stitcher 以"画布末端对应上一帧哪一行"
// 为不变量拼接,底栏估计增减都不会丢行或重复。
class FixedRegionDetector {
public:
    struct Runs {
        int top = 0;
        int bottom = 0;
    };

    static Runs measure(const QImage& prevGray, const QImage& curGray, int ignoreRight);

    void addSample(Runs raw, int frameHeight);

    int top() const { return top_; }
    int bottom() const { return bottom_; }
    int samples() const { return samples_; }

private:
    std::array<Runs, 3> recent_{};
    int samples_ = 0;
    int top_ = 0;
    int bottom_ = 0;
};

} // namespace pixora
