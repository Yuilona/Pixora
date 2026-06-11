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
// 状态序列化与分组随 M5(见 ARCHITECTURE §5.4)。
class PinWindow : public QWidget {
    Q_OBJECT
public:
    PinWindow(const QImage& image, const QPoint& topLeftLogical,
              ISystemIntegration* system);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QSize scaledSize() const;
    void applyScale(qreal scale);

    QImage image_;       // 含 DPR 标记
    ISystemIntegration* system_;
    qreal scale_ = 1.0;
    QPoint dragOffset_;
};

} // namespace pixora
