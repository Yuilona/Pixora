#pragma once

#include "core/stitch/FixedRegionDetector.h"

#include <QImage>

namespace pixora {

// 长截图拼接器:从上一帧底部取模板带,在新帧中用归一化互相关
// (NCC, OpenCV matchTemplate)定位垂直偏移,增量拼接到画布
// (见 ARCHITECTURE §5.3.2)。
//
// 鲁棒性:
// - 三条水平模板带分别匹配,偏移取中位数 → 抵抗局部动画干扰;
// - 固定区域检测:sticky header 排除出搜索范围(防误匹配),
//   sticky footer 从增量中裁剪、成图时只在底部保留一次。
//
// 纯算法模块:输入帧序列 → 输出长图,不依赖 Qt 事件循环,
// 便于用合成图像与录制帧序列做回归测试。
// 输入帧必须同宽同高(物理像素)。
class Stitcher {
public:
    struct Config {
        int templateStripHeight = 60; // 模板带高度(px)
        int bottomGuard = 12;         // 底部保护带:避开闪烁光标/状态栏
        int rightGuard = 24;          // 右侧保护带:避开滚动条
        // NCC 阈值。真实场景中 ClearType/分数缩放导致同一内容两次
        // 渲染并非逐字节一致,阈值过高会永久失配无法恢复。
        double minMatchScore = 0.80;
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
    FixedRegionDetector fixedDetector_;
    bool footerTrimmed_ = false; // 画布尾部的首帧底栏是否已剔除
};

} // namespace pixora
