#pragma once

#include "app/OutputService.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace pixora {

class IScreenCapturer;
class IWindowEnumerator;
class OverlayWindow;
class SnipSession;

// 截图编排:热键/托盘触发 → 冻结全部屏幕 → 每屏遮罩窗 →
// 选区确认后经 OutputService 输出(见 ARCHITECTURE §8 数据流)。
// enumerator 可为 nullptr(平台无实现)→ 无窗口吸附,仅自由选区。
class CaptureService : public QObject {
    Q_OBJECT
public:
    CaptureService(IScreenCapturer& capturer, IWindowEnumerator* enumerator,
                   QObject* parent = nullptr);
    ~CaptureService() override;

    void start();
    bool isActive() const { return session_ != nullptr; }

signals:
    void copiedToClipboard();
    void savedToFile(const QString& path);

private:
    void teardown();

    IScreenCapturer& capturer_;
    IWindowEnumerator* enumerator_;
    OutputService output_;
    std::unique_ptr<SnipSession> session_;
    std::vector<OverlayWindow*> overlays_;
};

} // namespace pixora
