#pragma once

#include "ui/overlay/SelectionHandles.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

class QLineEdit;

namespace pixora {

class IElementLocator;
class SnipSession;
struct ScreenSnap;

// 单屏遮罩窗:全屏显示冻结画面 + 暗化遮罩 + 选区交互。
// 每个屏幕一个实例,规避跨 DPI 渲染缩放问题;选区/悬停状态(全局逻辑
// 坐标)统一存于 SnipSession,本窗只做显示与输入转发(见 ARCHITECTURE §5.1)。
//
// 交互:悬停自动吸附窗口(按住 Shift 吸附窗口内 UI 元素)→
// 单击吸附为选区 / 拖拽自由选区 → 手柄缩放、内部拖动平移 →
// Enter/双击复制, Ctrl+S 另存, Esc 取消。
// elementLocator 可为 nullptr(平台无实现)→ 仅窗口级吸附。
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    // colorPickOnly:取色专用模式——不框选/不标注,左键单击即取光标处色值
    // (Shift+单击取 RGB),Esc 退出;遮罩不暗化、不显示工具栏。
    OverlayWindow(const ScreenSnap& snap, SnipSession& session,
                  IElementLocator* elementLocator = nullptr,
                  bool colorPickOnly = false);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Mode {
        Idle,
        Creating,
        Moving,
        Resizing,
        Drawing,
        DraggingItem,
        ResizingItem,    // 选中条目的手柄缩放(矩形/椭圆/马赛克/模糊)
        DraggingArrowEnd // 选中箭头的端点拖拽
    };

    QRect selectionLocal() const;
    QRect toLocal(const QRect& globalRect) const;
    QRect selectedItemHandleRect() const; // 选中条目手柄区(本窗局部坐标)
    bool magnifierVisible() const;
    void pickColorAt(const QPoint& localPos, bool rgb); // 取色:采样并结束会话
    void nudgeSelection(QKeyEvent* event); // 方向键微调选区(Ctrl 调大小)
    void updateElementHover(const QPoint& globalLogical,
                            bool force = false); // Shift 元素级吸附
    void startTextEditing(const QPoint& globalPos, bool editExisting);
    void finishTextEditing(bool accept);
    void addBadge(const QPoint& globalPos);

    SnipSession& session_;
    IElementLocator* elementLocator_;
    bool colorPickOnly_ = false;
    QElapsedTimer elementThrottle_; // 元素查询节流(UIA 可能较慢)
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
    QLineEdit* textEditor_ = nullptr;
    QPoint textPosGlobal_;
    bool editingExistingText_ = false; // 双击文字条目的二次编辑
    QPoint lastDragGlobal_; // 条目拖动:上一移动点(增量)与累计位移
    QPoint itemDragTotal_;
    QRect baseItemRect_;    // 条目缩放:按下时的基准几何
    QPoint baseArrowFrom_;  // 箭头端点拖拽:按下时的两端
    QPoint baseArrowTo_;
    bool draggingArrowFrom_ = false;
};

} // namespace pixora
