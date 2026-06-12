#pragma once

#include <QPixmap>
#include <QPropertyAnimation>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace pixora {

// 自绘通知弹窗:右下角深色圆角卡片(logo + 标题 + 正文)。
// 取代系统托盘气泡——后者强制带 "pixora.exe" 署名行且样式不可控。
// 不抢焦点;自动淡出;点击立即关闭;新通知复用同一窗口并重置计时。
class ToastWindow : public QWidget {
    Q_OBJECT
public:
    ToastWindow();

    void popup(const QString& title, const QString& message);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void startFadeOut();

    QString title_;
    QString message_;
    QPixmap logo_;
    QTimer hideTimer_;
    QPropertyAnimation fade_;
};

} // namespace pixora
