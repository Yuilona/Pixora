#pragma once

#include "app/OutputService.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace pixora {

class IScreenCapturer;
class OverlayWindow;
class SnipSession;

// 截图编排:热键/托盘触发 → 冻结全部屏幕 → 每屏遮罩窗 →
// 选区确认后经 OutputService 输出(见 ARCHITECTURE §8 数据流)。
class CaptureService : public QObject {
    Q_OBJECT
public:
    explicit CaptureService(IScreenCapturer& capturer, QObject* parent = nullptr);
    ~CaptureService() override;

    void start();
    bool isActive() const { return session_ != nullptr; }

signals:
    void copiedToClipboard();
    void savedToFile(const QString& path);

private:
    void teardown();

    IScreenCapturer& capturer_;
    OutputService output_;
    std::unique_ptr<SnipSession> session_;
    std::vector<OverlayWindow*> overlays_;
};

} // namespace pixora
