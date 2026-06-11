#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

namespace pixora {

class ISystemIntegration;

// 贴图窗:无边框置顶悬浮显示一张图像。
// 交互:左键拖动移动 / 滚轮缩放(10%–500%)/ Ctrl+滚轮调透明度 /
// 双击或 Esc 关闭 / 右键菜单(复制、另存、点击穿透、关闭)。
// 点击穿透开启后本窗不再接收鼠标,经托盘"关闭所有贴图"退出。
// 持久化由 PinService 负责:状态变化发 stateChanged,用户主动
// 关闭发 closedByUser(退出程序不触发 → 下次启动可恢复)。
class PinWindow : public QWidget {
    Q_OBJECT
public:
    PinWindow(const QImage& image, const QPoint& topLeftLogical,
              ISystemIntegration* system);

    const QImage& image() const { return image_; }
    qreal scale() const { return scale_; }
    void restoreState(qreal scale, qreal opacity); // 启动恢复:缩放+透明度

signals:
    void stateChanged();              // 位置/缩放/透明度变化(由服务端防抖落盘)
    void closedByUser(PinWindow* pin);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QSize scaledSize() const;
    void applyScale(qreal scale);

    QImage image_;       // 含 DPR 标记
    ISystemIntegration* system_;
    qreal scale_ = 1.0;
    QPoint dragOffset_;
};

} // namespace pixora
