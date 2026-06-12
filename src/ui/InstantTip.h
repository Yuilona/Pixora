#pragma once

#include <QCursor>
#include <QEvent>
#include <QObject>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

namespace pixora {

// 无焦点悬浮窗(WindowDoesNotAcceptFocus)上 Qt 默认 tooltip 通道不可靠,
// 且点击时提示会被自动隐藏——统一接管:进入即显(无延迟)、
// 松开重显(此时提示文本可能已更新,如粗细档位)、离开即藏。
// 用法:btn->installEventFilter(tipFilter);提示文本仍走 setToolTip。
class InstantTip : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* w = qobject_cast<QWidget*>(watched);
        if (!w || w->toolTip().isEmpty()) {
            return QObject::eventFilter(watched, event);
        }
        switch (event->type()) {
        case QEvent::Enter:
            QToolTip::showText(QCursor::pos() + QPoint(0, 16), w->toolTip(), w);
            break;
        case QEvent::MouseButtonRelease:
            // 延后到事件处理完,提示文本此时已是新状态;
            // 点的是出口按钮时工具条已隐藏,不能再把提示弹回来
            QTimer::singleShot(0, w, [w] {
                if (w->isVisible()) {
                    QToolTip::showText(QCursor::pos() + QPoint(0, 16), w->toolTip(), w);
                }
            });
            break;
        case QEvent::Leave:
            QToolTip::hideText();
            break;
        case QEvent::Hide:
            // 提示是独立顶层窗,宿主(随会话结束)隐藏时不会自动跟着藏
            QToolTip::hideText();
            break;
        case QEvent::ToolTip:
            return true; // 接管默认 tooltip 通道,避免双重显示
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace pixora
