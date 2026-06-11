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
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace pixora {

namespace {
const QColor kFrameColor(45, 124, 246);
constexpr qreal kMinScale = 0.1;
constexpr qreal kMaxScale = 5.0;
constexpr int kFoldedHeight = 26;          // 折叠小条高度
constexpr qint64 kDisplayPixelCap = 8'000'000; // 超过则降采样显示
} // namespace

PinWindow::PinWindow(const QImage& image, const QPoint& topLeftLogical,
                     ISystemIntegration* system)
    : image_(image), system_(system) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Pixora 贴图"));
    rebuildDisplayCache();
    resize(scaledSize());
    move(topLeftLogical);
}

void PinWindow::rebuildDisplayCache() {
    const qint64 pixels = qint64(image_.width()) * image_.height();
    if (pixels <= kDisplayPixelCap) {
        display_ = image_;
        return;
    }
    // 等比降采样到上限像素数;长截图贴出后拖动/重绘不再卡
    const qreal factor = std::sqrt(qreal(kDisplayPixelCap) / qreal(pixels));
    display_ = image_.scaled(std::max(1, qRound(image_.width() * factor)),
                             std::max(1, qRound(image_.height() * factor)),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    display_.setDevicePixelRatio(image_.devicePixelRatio());
}

QSize PinWindow::scaledSize() const {
    const QSizeF logical = image_.deviceIndependentSize();
    return (logical * scale_).toSize().expandedTo(QSize(24, 24));
}

void PinWindow::applyGeometryForState() {
    if (folded_) {
        resize(std::clamp(scaledSize().width(), 80, 240), kFoldedHeight);
    } else {
        resize(scaledSize());
    }
    update();
}

void PinWindow::applyScale(qreal scale) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
    resize(scaledSize());
    update();
    emit stateChanged();
}

void PinWindow::restoreState(qreal scale, qreal opacity, bool folded) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
    folded_ = folded;
    setWindowOpacity(std::clamp(opacity, 0.2, 1.0));
    applyGeometryForState();
}

void PinWindow::toggleFolded() {
    folded_ = !folded_;
    applyGeometryForState();
    emit stateChanged();
}

void PinWindow::rotate90() {
    const qreal dpr = image_.devicePixelRatio();
    image_ = image_.transformed(QTransform().rotate(90));
    image_.setDevicePixelRatio(dpr);
    rebuildDisplayCache();
    applyGeometryForState();
    emit imageChanged();
    emit stateChanged();
}

void PinWindow::flipHorizontal() {
    image_ = image_.flipped(Qt::Horizontal);
    rebuildDisplayCache();
    update();
    emit imageChanged();
}

void PinWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (folded_) {
        // 小条:图像顶部按宽度等比铺放,溢出裁剪
        const QSizeF s = display_.deviceIndependentSize();
        const qreal h = s.width() > 0 ? s.height() * width() / s.width() : height();
        painter.drawImage(QRectF(0, 0, width(), h), display_);
        painter.fillRect(rect(), QColor(0, 0, 0, 60));
    } else {
        painter.drawImage(rect(), display_);
    }
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
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (folded_) {
        toggleFolded(); // 折叠态双击=展开,避免误关
    } else {
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
    if (folded_) {
        return; // 折叠态不缩放
    }
    const qreal factor = std::pow(1.1, event->angleDelta().y() / 120.0);
    applyScale(scale_ * factor);
}

void PinWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_Space:
        toggleFolded();
        return;
    case Qt::Key_R:
        rotate90();
        return;
    case Qt::Key_H:
        flipHorizontal();
        return;
    default:
        QWidget::keyPressEvent(event);
    }
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
    menu.addAction(folded_ ? QStringLiteral("展开\tSpace")
                           : QStringLiteral("折叠为小条\tSpace"),
                   [this] { toggleFolded(); });
    menu.addAction(QStringLiteral("旋转 90°\tR"), [this] { rotate90(); });
    menu.addAction(QStringLiteral("水平翻转\tH"), [this] { flipHorizontal(); });
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
