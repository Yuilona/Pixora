#include "ui/overlay/OverlayWindow.h"

#include "core/capture/SnipSession.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace pixora {

namespace {
const QColor kMaskColor(0, 0, 0, 120);
const QColor kBorderColor(45, 124, 246); // Snipaste 风格蓝
} // namespace

OverlayWindow::OverlayWindow(const ScreenSnap& snap, SnipSession& session)
    : session_(session), frozen_(snap.image) {
    frozen_.setDevicePixelRatio(snap.dpr);

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setGeometry(snap.geometryLogical);
    setCursor(Qt::CrossCursor);

    connect(&session_, &SnipSession::selectionChanged, this,
            qOverload<>(&QWidget::update));
}

QRect OverlayWindow::selectionLocal() const {
    if (!session_.hasSelection()) {
        return {};
    }
    return session_.selection().translated(-geometry().topLeft()) & rect();
}

void OverlayWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.drawImage(0, 0, frozen_);

    // 选区外暗化
    const QRect sel = selectionLocal();
    QRegion dimmed(rect());
    if (!sel.isEmpty()) {
        dimmed -= sel;
    }
    painter.setClipRegion(dimmed);
    painter.fillRect(rect(), kMaskColor);
    painter.setClipping(false);

    if (sel.isEmpty()) {
        return;
    }
    painter.setPen(QPen(kBorderColor, 2));
    painter.drawRect(QRectF(sel).adjusted(0.5, 0.5, -0.5, -0.5));

    // 尺寸标签(逻辑像素)
    const QRect global = session_.selection();
    const QString label = QStringLiteral("%1 × %2").arg(global.width()).arg(global.height());
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    const QRect textRect = painter.fontMetrics().boundingRect(label).adjusted(-6, -3, 6, 3);
    QPoint anchor(sel.left(), sel.top() - textRect.height() - 4);
    if (anchor.y() < 0) {
        anchor.setY(sel.top() + 4);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRoundedRect(QRect(anchor, textRect.size()), 3, 3);
    painter.setPen(Qt::white);
    painter.drawText(QRect(anchor, textRect.size()), Qt::AlignCenter, label);
}

void OverlayWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = true;
    dragAnchorGlobal_ = event->globalPosition().toPoint();
    session_.setSelection(QRect(dragAnchorGlobal_, QSize()));
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        return;
    }
    session_.setSelection(QRect(dragAnchorGlobal_, event->globalPosition().toPoint()));
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
    }
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton &&
        session_.selection().contains(event->globalPosition().toPoint())) {
        session_.confirm();
    }
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        session_.cancel();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        session_.confirm();
        break;
    case Qt::Key_S:
        if (event->modifiers() & Qt::ControlModifier) {
            session_.requestSave();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

} // namespace pixora
