#pragma once

#include "app/OutputService.h"
#include "core/stitch/Stitcher.h"

#include <QImage>
#include <QObject>
#include <QRect>
#include <QTimer>

class QScreen;

namespace pixora {

class HistoryService;
class IInputInjector;
class IScreenCapturer;
class RegionIndicator;
class ScrollCaptureBar;
class ScrollPreview;
class SettingsService;

// 长截图编排(见 ARCHITECTURE §5.3.1):
// 选区来自截图工具栏的[长截图]按钮(CaptureService 拆遮罩后移交)→
// 区域指示框 + 控制条 → ~15fps 抓帧喂给 Stitcher(无位移帧由拼接器
// 丢弃)→ 完成后经所选出口输出。拼接中再按截图热键等同点[复制]。
//
// 手动模式(默认):用户自己滚动。
// 自动模式(injector 可用时,控制条开关):注入滚轮 → 等待画面稳定 →
// 抓帧拼接;连续多次无新内容(到底)自动完成,连续匹配失败自动回手动。
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
    void copiedToClipboard(int logicalHeight);
    void pinCaptured(const QImage& image, const QPoint& topLeftLogical);
    void savedToFile(const QString& path);

private:
    void tick();
    void tickAuto(const QImage& frame);
    void handleAppend(Stitcher::AppendResult result);
    void finishCapture(Outlet outlet = Outlet::Copy);
    void teardownScroll();

    IScreenCapturer& capturer_;
    IInputInjector* injector_;
    OutputService output_;
    HistoryService* history_;

    QScreen* screen_ = nullptr;
    QRect regionGlobal_;
    Stitcher stitcher_;
    QTimer timer_;
    QImage lastGrab_;
    int frames_ = 0;
    RegionIndicator* indicator_ = nullptr;
    ScrollCaptureBar* bar_ = nullptr;
    ScrollPreview* preview_ = nullptr; // 两侧放不下时为空
    qint64 lastPreviewMs_ = 0;         // 预览节流(手动快滚时限频)
    void updatePreview();

    // 录帧(回归样本采集):PIXORA_RECORD_FRAMES=目录 时,把每个
    // 喂给拼接器的帧和最终成图存盘,直接作为 tests/fixtures 用例
    QString recordDir_;
    int recordIndex_ = 0;
    void recordFrame(const QImage& frame);

    // 自动模式状态
    enum class Driver { Wheel, PageDown }; // 滚轮无效时自动切 PageDown
    Driver driver_ = Driver::Wheel;
    void injectStep();
    bool autoMode_ = false;
    bool awaitingStable_ = false; // 已注入滚轮,等待画面稳定
    int stableWaitMs_ = 0;
    int sinceInjectMs_ = 0;       // 距上次注入的时间(节奏限制用)
    QImage prevTickFrame_;
    int noNewStreak_ = 0;
    int failStreak_ = 0;
};

} // namespace pixora
