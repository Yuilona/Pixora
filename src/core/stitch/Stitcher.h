#pragma once

#include <QImage>

namespace pixora {

// 长截图拼接器:从上一帧底部取模板带,在新帧中用归一化互相关
// (NCC, OpenCV matchTemplate)定位垂直偏移,增量拼接到画布
// (见 ARCHITECTURE §5.3.2)。
//
// 纯算法模块:输入帧序列 → 输出长图,不依赖 Qt 事件循环,
// 便于用合成图像与录制帧序列做回归测试。
// 输入帧必须同宽同高(物理像素)。
// M3 第一刀:单模板带;三带中位数与固定区域检测随后续切片。
class Stitcher {
public:
    struct Config {
        int templateStripHeight = 60; // 模板带高度(px)
        int bottomGuard = 12;         // 底部保护带:避开闪烁光标/状态栏
        int rightGuard = 24;          // 右侧保护带:避开滚动条
        double minMatchScore = 0.90;  // NCC 阈值,低于判为失败帧
        int maxCanvasHeight = 32000;  // 画布上限保护
    };

    enum class AppendResult { Appended, NoNewContent, MatchFailed };

    explicit Stitcher(Config config = {});

    void begin(const QImage& firstFrame);
    AppendResult append(const QImage& frame);

    bool active() const { return usedHeight_ > 0; }
    int resultHeight() const { return usedHeight_; }
    QImage result() const;

private:
    void ensureCapacity(int neededHeight);

    Config config_;
    QImage canvas_; // 预分配增长式画布(2x 扩容)
    int usedHeight_ = 0;
    QImage lastFrame_;
};

} // namespace pixora
