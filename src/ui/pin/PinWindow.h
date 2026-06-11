#pragma once

#include <QImage>
#include <QPoint>
#include <QWidget>

namespace pixora {

class ISystemIntegration;

// 贴图窗:无边框置顶悬浮显示一张图像。
// 交互:左键拖动移动 / 滚轮缩放(10%–500%)/ Ctrl+滚轮调透明度 /
// 双击或 Esc 关闭(折叠态双击=展开)/ Space 折叠为小条 /
// R 旋转 90° / H 水平翻转 / 右键菜单(复制、另存、折叠、旋转、
// 翻转、点击穿透、关闭)。
// 点击穿透开启后本窗不再接收鼠标,经托盘"关闭所有贴图"退出。
// 持久化由 PinService 负责:状态变化发 stateChanged,图像内容变化
// (旋转/翻转)发 imageChanged,用户主动关闭发 closedByUser。
class PinWindow : public QWidget {
    Q_OBJECT
public:
    PinWindow(const QImage& image, const QPoint& topLeftLogical,
              ISystemIntegration* system);

    const QImage& image() const { return image_; }
    qreal scale() const { return scale_; }
    bool isFolded() const { return folded_; }
    void restoreState(qreal scale, qreal opacity, bool folded); // 启动恢复

signals:
    void stateChanged();              // 位置/缩放/透明度/折叠变化(防抖落盘)
    void imageChanged();              // 旋转/翻转后图像本体变化(重存 PNG)
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
    void toggleFolded();
    void rotate90();
    void flipHorizontal();
    void rebuildDisplayCache(); // 超大图降采样显示(原图保留用于复制/另存)
    void applyGeometryForState();

    QImage image_;    // 含 DPR 标记;复制/另存用原图
    QImage display_;  // 绘制用(大图为降采样副本)
    ISystemIntegration* system_;
    qreal scale_ = 1.0;
    bool folded_ = false;
    QPoint dragOffset_;
};

} // namespace pixora
