#pragma once

#include "ui/overlay/SelectionHandles.h"

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

namespace pixora {

class SnipSession;
struct ScreenSnap;

// 单屏遮罩窗:全屏显示冻结画面 + 暗化遮罩 + 选区交互。
// 每个屏幕一个实例,规避跨 DPI 渲染缩放问题;选区/悬停状态(全局逻辑
// 坐标)统一存于 SnipSession,本窗只做显示与输入转发(见 ARCHITECTURE §5.1)。
//
// 交互:悬停自动吸附窗口 → 单击吸附为选区 / 拖拽自由选区 →
// 手柄缩放、内部拖动平移 → Enter/双击复制, Ctrl+S 另存, Esc 取消。
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
    enum class Mode { Idle, Creating, Moving, Resizing, Drawing };

    QRect selectionLocal() const;
    QRect toLocal(const QRect& globalRect) const;
    bool magnifierVisible() const;

    SnipSession& session_;
    QImage frozen_;     // 含 DPR 标记,按逻辑尺寸绘制
    QImage physical_;   // DPR=1 裸图,放大镜取样用
    qreal dpr_ = 1.0;

    Mode mode_ = Mode::Idle;
    SelectionHandles::Hit activeHandle_ = SelectionHandles::Hit::None;
    QPoint pressGlobal_;
    QPoint grabOffset_;
    QRect baseSelection_;
    bool moved_ = false;
    QPoint cursorLocal_;
    bool hasCursor_ = false;
};

} // namespace pixora
