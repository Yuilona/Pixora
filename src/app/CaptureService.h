#pragma once

#include "app/OutputService.h"

#include <QObject>
#include <QRect>
#include <QString>

#include <memory>
#include <vector>

namespace pixora {

class AnnotationToolbar;
class HistoryService;
class IElementLocator;
class IScreenCapturer;
class IWindowEnumerator;
class OverlayWindow;
class SettingsService;
class SnipSession;

// 截图编排:热键/托盘触发 → 冻结全部屏幕 → 每屏遮罩窗 →
// 选区确认后经 OutputService 输出(见 ARCHITECTURE §8 数据流)。
// enumerator 可为 nullptr(平台无实现)→ 无窗口吸附,仅自由选区。
// history / elementLocator 可为 nullptr → 不留底 / 仅窗口级吸附。
class CaptureService : public QObject {
    Q_OBJECT
public:
    CaptureService(IScreenCapturer& capturer, IWindowEnumerator* enumerator,
                   IElementLocator* elementLocator, const SettingsService* settings,
                   HistoryService* history, QObject* parent = nullptr);
    ~CaptureService() override;

    void start();
    // 以上次提交过的选区直接发起截图并进入确认态(工具栏即出);
    // 无历史选区或其落点已不在当前屏幕范围内时退化为普通 start()。
    void startRepeatLastRegion();
    // 取色专用会话:遮罩不暗化、无工具栏,左键单击取光标处色值即退出。
    void startColorPickOnly();
    bool isActive() const { return session_ != nullptr; }

signals:
    void copiedToClipboard();
    void savedToFile(const QString& path);
    void pinCaptured(const QImage& image, const QPoint& topLeftLogical);
    // 工具栏[长截图]:拆掉遮罩后把选区交给 ScrollCaptureService 接管
    void scrollCaptureRequested(const QRect& regionGlobal);
    void colorCopied(const QString& text); // 取色器出口(C 键)
    // 工具栏[提取文字]/[翻译]:选区原始图(不含标注)交给 ScreenTextService
    void ocrRequested(const QImage& image);
    void translateRequested(const QImage& image, const QRect& regionGlobal);

private:
    void beginSession(bool colorPickOnly); // start / startColorPickOnly 共用建场
    QImage renderResult(const QRect& region) const; // 截取 + 平铺标注
    void teardown();

    QImage renderAndRecord(const QRect& region); // 截取+平铺标注+历史留底

    IScreenCapturer& capturer_;
    IWindowEnumerator* enumerator_;
    IElementLocator* elementLocator_;
    OutputService output_;
    HistoryService* history_;
    std::unique_ptr<SnipSession> session_;
    std::vector<OverlayWindow*> overlays_;
    AnnotationToolbar* toolbar_ = nullptr;
    QRect lastRegion_; // 上次提交的选区(全局逻辑坐标),供"重做上次选区"复用
};

} // namespace pixora
