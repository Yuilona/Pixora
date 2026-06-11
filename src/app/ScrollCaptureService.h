#pragma once

#include "app/OutputService.h"
#include "core/stitch/Stitcher.h"

#include <QImage>
#include <QObject>
#include <QRect>
#include <QTimer>

#include <memory>
#include <vector>

class QScreen;

namespace pixora {

class IInputInjector;
class IScreenCapturer;
class IWindowEnumerator;
class OverlayWindow;
class RegionIndicator;
class ScrollCaptureBar;
class SettingsService;
class SnipSession;

// 长截图编排(见 ARCHITECTURE §5.3.1):
// F2 → 复用遮罩选区确定目标区域(支持窗口吸附)→ 区域指示框 + 控制条
// → ~15fps 抓帧喂给 Stitcher(无位移帧由拼接器丢弃)→ 完成后复制。
// 再按一次 F2 等同点[完成]。
//
// 手动模式(默认):用户自己滚动。
// 自动模式(injector 可用时,控制条开关):注入滚轮 → 等待画面稳定 →
// 抓帧拼接;连续多次无新内容(到底)自动完成,连续匹配失败自动回手动。
class ScrollCaptureService : public QObject {
    Q_OBJECT
public:
    ScrollCaptureService(IScreenCapturer& capturer, IWindowEnumerator* enumerator,
                         IInputInjector* injector, const SettingsService* settings,
                         QObject* parent = nullptr);
    ~ScrollCaptureService() override;

    void start();

    enum class Outlet { Copy, Pin, Save };

signals:
    void copiedToClipboard(int logicalHeight);
    void pinCaptured(const QImage& image, const QPoint& topLeftLogical);
    void savedToFile(const QString& path);

private:
    void beginScrollPhase(QRect regionGlobal);
    void tick();
    void tickAuto(const QImage& frame);
    void handleAppend(Stitcher::AppendResult result);
    void finishCapture(Outlet outlet = Outlet::Copy);
    void teardownSelection();
    void teardownScroll();

    IScreenCapturer& capturer_;
    IWindowEnumerator* enumerator_;
    IInputInjector* injector_;
    OutputService output_;

    // 选区阶段
    std::unique_ptr<SnipSession> session_;
    std::vector<OverlayWindow*> overlays_;

    // 滚动阶段
    QScreen* screen_ = nullptr;
    QRect regionGlobal_;
    Stitcher stitcher_;
    QTimer timer_;
    QImage lastGrab_;
    int frames_ = 0;
    RegionIndicator* indicator_ = nullptr;
    ScrollCaptureBar* bar_ = nullptr;

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
