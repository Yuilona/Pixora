#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

namespace pixora {

// 长截图控制条:显示拼接状态,提供自动滚动开关与完成/取消按钮。
// 不抢焦点,不遮挡捕获区域(置于区域下方,放不下则上方)。
class ScrollCaptureBar : public QWidget {
    Q_OBJECT
public:
    ScrollCaptureBar(const QRect& regionGlobal, const QRect& virtualBounds,
                     bool autoModeAvailable);

    void setStatus(const QString& text);
    void setAutoChecked(bool checked);

signals:
    void autoToggled(bool enabled);
    void finishRequested();      // 复制(默认出口)
    void finishPinRequested();   // 贴图
    void finishSaveRequested();  // 另存
    void cancelRequested();

private:
    QLabel* status_ = nullptr;
    QToolButton* autoBtn_ = nullptr;
};

} // namespace pixora
