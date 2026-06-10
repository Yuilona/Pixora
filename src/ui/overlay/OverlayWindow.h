#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

namespace pixora {

class SnipSession;
struct ScreenSnap;

// 单屏遮罩窗:全屏显示冻结画面 + 暗化遮罩 + 选区交互。
// 每个屏幕一个实例,规避跨 DPI 渲染缩放问题;选区状态(全局逻辑坐标)
// 统一存于 SnipSession,本窗只做显示与输入转发(见 ARCHITECTURE §5.1)。
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    OverlayWindow(const ScreenSnap& snap, SnipSession& session);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QRect selectionLocal() const; // 会话全局选区 → 本窗局部坐标

    SnipSession& session_;
    QImage frozen_;     // 含 DPR 标记,按逻辑尺寸绘制
    QPoint dragAnchorGlobal_;
    bool dragging_ = false;
};

} // namespace pixora
