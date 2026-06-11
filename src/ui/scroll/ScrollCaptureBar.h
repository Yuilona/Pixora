#pragma once

#include <QWidget>

class QLabel;

namespace pixora {

// 长截图控制条:显示拼接状态,提供完成/取消按钮。
// 不抢焦点,不遮挡捕获区域(置于区域下方,放不下则上方)。
class ScrollCaptureBar : public QWidget {
    Q_OBJECT
public:
    ScrollCaptureBar(const QRect& regionGlobal, const QRect& virtualBounds);

    void setStatus(const QString& text);

signals:
    void finishRequested();
    void cancelRequested();

private:
    QLabel* status_ = nullptr;
};

} // namespace pixora
