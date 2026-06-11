#include "ui/pin/PinWindow.h"

#include "app/OutputService.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
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

PinWindow::PinWindow(const QImage& image, const QPoint& topLeftLogical) : image_(image) {
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
    menu.addSeparator();
    menu.addAction(QStringLiteral("关闭贴图"), [this] { close(); });
    menu.exec(event->globalPos());
}

} // namespace pixora
