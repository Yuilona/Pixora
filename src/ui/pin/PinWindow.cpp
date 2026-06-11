#include "ui/pin/PinWindow.h"

#include "app/OutputService.h"
#include "platform/interface/SystemIntegration.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace pixora {

namespace {
const QColor kFrameColor(45, 124, 246);
constexpr qreal kMinScale = 0.1;
constexpr qreal kMaxScale = 5.0;
} // namespace

PinWindow::PinWindow(const QImage& image, const QPoint& topLeftLogical,
                     ISystemIntegration* system)
    : image_(image), system_(system) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Pixora 贴图"));
    resize(scaledSize());
    move(topLeftLogical);
}

QSize PinWindow::scaledSize() const {
    const QSizeF logical = image_.deviceIndependentSize();
    return (logical * scale_).toSize().expandedTo(QSize(24, 24));
}

void PinWindow::applyScale(qreal scale) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
    resize(scaledSize());
    update();
    emit stateChanged();
}

void PinWindow::restoreState(qreal scale, qreal opacity) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
    resize(scaledSize());
    setWindowOpacity(std::clamp(opacity, 0.2, 1.0));
}

void PinWindow::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    emit stateChanged();
}

void PinWindow::closeEvent(QCloseEvent* event) {
    // 仅用户主动关闭走到这里(Esc/双击/菜单/托盘关闭全部);
    // 程序退出不触发 closeEvent → 清单保留,下次启动恢复
    emit closedByUser(this);
    QWidget::closeEvent(event);
}

void PinWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(rect(), image_);
    painter.setPen(QPen(kFrameColor, 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragOffset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragOffset_);
    }
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        close();
    }
}

void PinWindow::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+滚轮:透明度 20%–100%
        const qreal delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
        setWindowOpacity(std::clamp(windowOpacity() + delta, 0.2, 1.0));
        emit stateChanged();
        return;
    }
    const qreal factor = std::pow(1.1, event->angleDelta().y() / 120.0);
    applyScale(scale_ * factor);
}

void PinWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.addAction(QStringLiteral("复制图像"), [this] {
        QGuiApplication::clipboard()->setImage(image_);
    });
    menu.addAction(QStringLiteral("另存为…"), [this] {
        OutputService output;
        output.saveWithDialog(image_);
    });
    if (system_) {
        menu.addSeparator();
        menu.addAction(QStringLiteral("点击穿透(经托盘菜单关闭)"), [this] {
            system_->setClickThrough(windowHandle(), true);
        });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("关闭贴图"), [this] { close(); });
    menu.exec(event->globalPos());
}

} // namespace pixora
