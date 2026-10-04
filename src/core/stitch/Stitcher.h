#pragma once

#include "core/stitch/FixedRegionDetector.h"
#include "core/stitch/RowCanvas.h"

#include <QImage>

#include <deque>

namespace pixora {

// 长截图拼接器(见 ARCHITECTURE §5.3.2)。逐帧:
//
// 1. 候选生成:三条竖带各自在上一帧里挑"有纹理"的模板带(自下而上,
//    可一直退到内容区顶部——大段留白/图片底色不再卡死),在行剖面
//    (水平块均值、垂直全分辨率)上做 NCC,取前几个峰作候选偏移 dy。
// 2. 全局校验:每个候选用整个重叠区(全宽全像素、隔行采样)的 NCC 打分,
//    择优并做 ±1 行精修;最优与次优(不同 dy)过近 → 周期性歧义,
//    拒配(宁可漏一帧也不重复内容)。局部动画只拉低分数,不改变名次。
// 3. 接缝回刷:新帧不只贡献新露出的底部 dy 行——在重叠区下半段找
//    新旧一致的行作接缝,接缝以下整段用新帧覆盖。于是懒加载占位图
//    被刷成加载后的样子,悬浮按钮/聊天气泡只在成图末尾留一次。
// 4. 不变量:记录"画布末端对应上一帧第几行"(lastContentBottom_),
//    底栏估计变化时按它对齐,不会丢行或重复。
//
// 纯算法模块,不依赖事件循环;输入帧须同宽同高(物理像素,32 位格式)。
class Stitcher {
public:
    struct Config {
        int templateStripHeight = 60; // 模板带高度(px)
        int bottomGuard = 12;         // 底部保护带:模板不取最底几行(光标/状态闪烁)
        int rightGuard = 24;          // 右侧保护带:匹配与校验都避开滚动条
        // 竖带模板的 NCC 阈值。ClearType/分数缩放下同一内容两次渲染并非
        // 逐字节一致,过高会永久失配
        double minMatchScore = 0.80;
        // 全重叠区校验分下限:局部动画占重叠区 1/4 时正确偏移约 0.75
        double minOverlapScore = 0.55;
        int maxCanvasHeight = 60000;          // 成图高度上限(JPEG 上限 65535)
        qint64 maxCanvasPixels = 80'000'000;  // 像素上限(约 320MB),宽区域据此限高
    };

    enum class AppendResult { Appended, NoNewContent, MatchFailed, LimitReached };

    // 不在类内用 "= {}" 默认参数:Config 的默认成员初始化器在
    // 未完成类上下文中,GCC/Clang 拒绝(MSVC 放行)
    Stitcher();
    explicit Stitcher(Config config);

    void begin(const QImage& firstFrame);
    AppendResult append(const QImage& frame);

    // 撤销最后一段拼接(画布、上一帧状态一并回退)。历史按内存预算保留
    bool undo();
    bool canUndo() const { return !undo_.empty(); }

    bool active() const { return canvas_.rows() > 0; }
    int resultHeight() const; // 成图高度(含末尾保留的底栏)
    QImage result() const;

    int lastScrollStep() const { return lastDy_; } // 最近一次成功拼接的位移(物理 px)
    int contentHeight() const;                     // 可滚动内容区高度(扣除固定头尾)
    int maxHeight() const { return maxRows_; }

    // 实时预览用:已拼内容的尾部 maxRows 行(物理像素)
    QImage tail(int maxRows) const;

private:
    struct UndoStep {
        int canvasRow = 0; // 该段接缝在画布中的行
        QImage backup;     // 被覆盖的画布行 [canvasRow, prevRows)
        int prevRows = 0;
        int prevSegmentStart = 0;
        QImage lastFrame;
        QImage lastGray;
        QImage lastProfile;
        int lastContentBottom = 0;
        int lastDy = 0;
        qint64 bytes() const;
    };

    struct Match {
        bool found = false;
        int dy = 0;
    };

    int searchWidth() const; // 匹配/校验用的列宽(扣右侧保护带)
    Match locate(const QImage& gray, const QImage& profile, int contentTop,
                 int contentBottom) const;
    int chooseSeam(const QImage& gray, int dy, int contentTop, int seamMax) const;
    // 画布截到 canvasRow,再接上 frame 的 [seam, newContentBottom) 行
    void overwriteFrom(const QImage& frame, int seam, int canvasRow, int newContentBottom);
    void pushUndo(UndoStep step);

    Config config_;
    RowCanvas canvas_;
    int maxRows_ = 0;
    QImage lastFrame_;
    QImage lastGray_;  // lastFrame_ 的灰度
    QImage lastProfile_; // lastGray_ 的行剖面(水平块均值)
    int lastContentBottom_ = 0; // 画布末端 ↔ lastFrame_ 的这一行
    int segmentStart_ = 0;      // 最后一段在画布中的起始行(原地回刷不越过它)
    int lastDy_ = 0;
    FixedRegionDetector fixedDetector_;
    std::deque<UndoStep> undo_;
};

} // namespace pixora
