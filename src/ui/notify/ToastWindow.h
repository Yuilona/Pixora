#pragma once

#include <QParallelAnimationGroup>
#include <QImage>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace pixora {

// 自绘通知弹窗:右下角深色圆角卡片(logo 或截图缩略图 + 标题 + 正文)。
// 取代系统托盘气泡——后者强制带 "pixora.exe" 署名行且样式不可控。
// 不抢焦点;右缘滑入/滑出 + 淡入淡出;点击立即关闭(带 link 时
// 先打开:网址走浏览器,本地文件夹走资源管理器,后者附"点击打开文件夹"提示);
// 鼠标悬停时暂停消失;新通知复用同一窗口并重置计时
// (已可见时直接换内容,不重播入场)。
class ToastWindow : public QWidget {
    Q_OBJECT
public:
    ToastWindow();

    // thumbnail 非空时以缩略图替代 logo(如刚复制的截图)
    void popup(const QString& title, const QString& message, const QString& link = {},
               const QImage& thumbnail = {});

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void startExit();

    QString title_;
    QString message_;
    QString link_; // 非空时点击打开
    QString hint_; // 可点击时的操作提示(主题蓝小字)
    QPixmap logo_;
    QPixmap thumb_; // 缩略图(空则画 logo)
    QTimer hideTimer_;

    QParallelAnimationGroup* enter_ = nullptr;
    QPropertyAnimation* enterPos_ = nullptr;
    QPropertyAnimation* enterOpacity_ = nullptr;
    QParallelAnimationGroup* exit_ = nullptr;
    QPropertyAnimation* exitPos_ = nullptr;
    QPropertyAnimation* exitOpacity_ = nullptr;
};

} // namespace pixora
