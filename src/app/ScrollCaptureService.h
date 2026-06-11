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

class IScreenCapturer;
class IWindowEnumerator;
class OverlayWindow;
class RegionIndicator;
class ScrollCaptureBar;
class SnipSession;

// 长截图编排(M3 手动模式,见 ARCHITECTURE §5.3.1):
// F2 → 复用遮罩选区确定目标区域(支持窗口吸附)→ 区域指示框 + 控制条
// → ~15fps 抓帧喂给 Stitcher(无位移帧由拼接器丢弃)→ 完成后复制。
// 再按一次 F2 等同点[完成]。自动模式(注入滚轮)随后续切片。
class ScrollCaptureService : public QObject {
    Q_OBJECT
public:
    ScrollCaptureService(IScreenCapturer& capturer, IWindowEnumerator* enumerator,
                         QObject* parent = nullptr);
    ~ScrollCaptureService() override;

    void start();

signals:
    void copiedToClipboard(int logicalHeight);

private:
    void beginScrollPhase(QRect regionGlobal);
    void tick();
    void finishCapture();
    void teardownSelection();
    void teardownScroll();

    IScreenCapturer& capturer_;
    IWindowEnumerator* enumerator_;
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
};

} // namespace pixora
