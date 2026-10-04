#pragma once

#include "app/OutputService.h"
#include "core/stitch/Stitcher.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QRect>
#include <QTimer>

#include <memory>

class QScreen;

namespace pixora {

class HistoryService;
class IFrameStream;
class IInputInjector;
class IScreenCapturer;
class RegionIndicator;
class ScrollCaptureBar;
class ScrollPreview;
class SettingsService;

// 长截图编排(见 ARCHITECTURE §5.3.1):
// 选区来自截图工具栏的[长截图]按钮(CaptureService 拆遮罩后移交)→
// 区域指示框 + 控制条 → ~30fps 经连续抓帧会话(Windows 走 DXGI)取帧,
// 有变化的帧喂给 Stitcher → 完成后经所选出口输出。拼接中再按截图热键
// 等同点[复制]。
//
// 手动模式(默认):用户自己滚动。
// 自动模式(injector 可用时,控制条开关):以"步"推进——注入一步滚动,
// 滚动动画期间的每个变化帧照常拼接(不必等画面完全静止,GIF/视频页面
// 不再每步卡满超时),画面安静下来即本步结束。每步位移按实测"每格
// 像素"校准到内容区高度的 60%。滚轮无效换 PageDown,PageDown 一页
// 超出选区(无重叠)则退回一页改用方向键;连续无新内容判定到底自动
// 完成,连续失配退回手动。
class ScrollCaptureService : public QObject {
    Q_OBJECT
public:
    ScrollCaptureService(IScreenCapturer& capturer, IInputInjector* injector,
                         const SettingsService* settings, HistoryService* history,
                         QObject* parent = nullptr);
    ~ScrollCaptureService() override;

    void start(const QRect& regionGlobal); // 全局逻辑坐标选区
    bool isActive() const { return timer_.isActive(); }
    void finish(); // 等同控制条[复制]

    enum class Outlet { Copy, Pin, Save };

signals:
    void copiedToClipboard(int logicalHeight, const QImage& image);
    void pinCaptured(const QImage& image, const QPoint& topLeftLogical);
    void savedToFile(const QString& path);

private:
    void tick();
    void beginStitch(const QImage& frame);
    void adaptFrameRate();
    void appendManual(const QImage& frame);
    void onAppended();
    void onLimitReached();
    void undoLastSegment();
    void setAutoMode(bool enabled);
    void finishCapture(Outlet outlet = Outlet::Copy);
    void teardownScroll();
    int logicalHeight() const;

    IScreenCapturer& capturer_;
    IInputInjector* injector_;
    OutputService output_;
    HistoryService* history_;

    QScreen* screen_ = nullptr;
    QRect regionGlobal_;
    std::unique_ptr<IFrameStream> stream_;
    Stitcher stitcher_;
    QTimer timer_;
    QImage lastGrab_;    // 最近一次有变化的帧
    int idleTicks_ = 0;  // 连续无变化 tick 数(GDI 抓帧时据此降帧)
    int frames_ = 0;
    bool limitReached_ = false;
    RegionIndicator* indicator_ = nullptr;
    ScrollCaptureBar* bar_ = nullptr;
    ScrollPreview* preview_ = nullptr; // 两侧放不下时为空
    qint64 lastPreviewMs_ = 0;         // 预览节流(手动快滚时限频)
    void updatePreview(bool force = false);

    // 录帧(回归样本采集):PIXORA_RECORD_FRAMES=目录 时,把每个
    // 喂给拼接器的帧和最终成图存盘,直接作为 tests/fixtures 用例
    QString recordDir_;
    int recordIndex_ = 0;
    void recordFrame(const QImage& frame);

    // 自动模式
    enum class Driver { Wheel, PageDown, ArrowKeys };
    void tickAuto(bool changed);
    void startStep();
    void finishStep();
    int stepUnits() const;
    void injectUnits(int units, bool down);
    void switchDriver(Driver driver, const QString& status);

    bool autoMode_ = false;
    Driver driver_ = Driver::Wheel;
    bool stepActive_ = false;
    QElapsedTimer stepClock_;
    int stepUnits_ = 0;      // 本步注入的格数/按键数
    int stepGrowth_ = 0;     // 本步成图增长(物理 px)
    bool stepFailed_ = false;
    int quietTicks_ = 0;     // 本步连续无变化 tick
    int stillFrames_ = 0;    // 本步连续"有变化但没增长"的帧(动画、懒加载)
    double pxPerUnit_ = 0.0; // 校准:每格滚轮/每次按键的实际位移(物理 px)
    double stepScale_ = 1.0; // 失配后缩小步幅
    int movedSteps_ = 0;     // 当前驱动下有位移的步数
    int noNewStreak_ = 0;
    int failStreak_ = 0;
};

} // namespace pixora
