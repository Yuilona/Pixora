#include "ui/pin/PinWindow.h"

#include "platform/interface/SystemIntegration.h"
#include "ui/Theme.h"

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
const QColor kFrameColor = theme::accent();
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
    setWindowTitle(tr("Pixora pin"));
    setMouseTracking(true); // 边缘热区光标反馈
    rebuildDisplayCache();
    resize(scaledSize());
    move(topLeftLogical);
}

PinWindow::Edge PinWindow::edgeAt(const QPoint& pos) const {
    if (folded_) {
        return Edge::None; // 折叠小条不缩放
    }
    constexpr int m = 6; // 边缘热区宽度
    const bool left = pos.x() <= m;
    const bool right = pos.x() >= width() - m;
    const bool top = pos.y() <= m;
    const bool bottom = pos.y() >= height() - m;
    if (top && left) return Edge::TopLeft;
    if (top && right) return Edge::TopRight;
    if (bottom && left) return Edge::BottomLeft;
    if (bottom && right) return Edge::BottomRight;
    if (left) return Edge::Left;
    if (right) return Edge::Right;
    if (top) return Edge::Top;
    if (bottom) return Edge::Bottom;
    return Edge::None;
}

Qt::CursorShape PinWindow::cursorForEdge(Edge edge) const {
    switch (edge) {
    case Edge::Left:
    case Edge::Right:
        return Qt::SizeHorCursor;
    case Edge::Top:
    case Edge::Bottom:
        return Qt::SizeVerCursor;
    case Edge::TopLeft:
    case Edge::BottomRight:
        return Qt::SizeFDiagCursor;
    case Edge::TopRight:
    case Edge::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Edge::None:
        break;
    }
    return Qt::ArrowCursor;
}

// 图像定比:任何边/角的拖拽都换算为统一 scale_;
// 拖左/上侧时锚定对侧边角不动,手感与普通窗口一致。
void PinWindow::performResize(const QPoint& globalPos) {
    const QPoint d = globalPos - pressGlobal_;
    const QSizeF imgLogical = image_.deviceIndependentSize();
    if (imgLogical.width() < 1 || imgLogical.height() < 1) {
        return;
    }

    qreal targetW = baseGeometry_.width();
    qreal targetH = baseGeometry_.height();
    const bool hasLeft = resizeEdge_ == Edge::Left || resizeEdge_ == Edge::TopLeft ||
                         resizeEdge_ == Edge::BottomLeft;
    const bool hasRight = resizeEdge_ == Edge::Right || resizeEdge_ == Edge::TopRight ||
                          resizeEdge_ == Edge::BottomRight;
    const bool hasTop = resizeEdge_ == Edge::Top || resizeEdge_ == Edge::TopLeft ||
                        resizeEdge_ == Edge::TopRight;
    const bool hasBottom = resizeEdge_ == Edge::Bottom ||
                           resizeEdge_ == Edge::BottomLeft ||
                           resizeEdge_ == Edge::BottomRight;
    if (hasRight) targetW += d.x();
    if (hasLeft) targetW -= d.x();
    if (hasBottom) targetH += d.y();
    if (hasTop) targetH -= d.y();

    qreal scale = scale_;
    const bool horizontal = hasLeft || hasRight;
    const bool vertical = hasTop || hasBottom;
    if (horizontal && vertical) {
        scale = std::max(targetW / imgLogical.width(), targetH / imgLogical.height());
    } else if (horizontal) {
        scale = targetW / imgLogical.width();
    } else if (vertical) {
        scale = targetH / imgLogical.height();
    }
    scale_ = std::clamp(scale, kMinScale, kMaxScale);

    const QSize sz = scaledSize();
    QPoint topLeft = baseGeometry_.topLeft();
    if (hasLeft) {
        topLeft.setX(baseGeometry_.right() - sz.width() + 1);
    }
    if (hasTop) {
        topLeft.setY(baseGeometry_.bottom() - sz.height() + 1);
    }
    setGeometry(QRect(topLeft, sz));
    update();
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

void PinWindow::setImage(const QImage& image) {
    if (image.isNull()) {
        return;
    }
    image_ = image;
    rebuildDisplayCache();
    applyGeometryForState();
    emit imageChanged();
}

void PinWindow::setStatusBadge(const QString& text) {
    badge_ = text;
    update();
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

    if (!badge_.isEmpty() && !folded_) {
        QFont f = font();
        f.setPixelSize(12);
        const QFontMetrics fm(f);
        const QRect r(width() - fm.horizontalAdvance(badge_) - 22, 6,
                      fm.horizontalAdvance(badge_) + 14, fm.height() + 6);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 150));
        painter.drawRoundedRect(r, 4, 4);
        painter.setFont(f);
        painter.setPen(Qt::white);
        painter.drawText(r, Qt::AlignCenter, badge_);
    }
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    resizeEdge_ = edgeAt(event->pos());
    if (resizeEdge_ != Edge::None) {
        resizing_ = true;
        baseGeometry_ = geometry();
        pressGlobal_ = event->globalPosition().toPoint();
        return;
    }
    dragOffset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        if (resizing_) {
            performResize(event->globalPosition().toPoint());
        } else {
            move(event->globalPosition().toPoint() - dragOffset_);
        }
        return;
    }
    setCursor(cursorForEdge(edgeAt(event->pos()))); // 悬停光标反馈
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && resizing_) {
        resizing_ = false;
        resizeEdge_ = Edge::None;
        emit stateChanged();
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
    theme::roundPopup(&menu); // 透明化弹层,QSS 圆角才完整
    menu.addAction(tr("Copy image"), [this] {
        QGuiApplication::clipboard()->setImage(image_);
    });
    menu.addAction(tr("Save as..."), [this] { emit saveRequested(image_); });
    menu.addSeparator();
    menu.addAction((folded_ ? tr("Unfold") : tr("Fold into a slim bar")) +
                       QStringLiteral("\tSpace"),
                   [this] { toggleFolded(); });
    menu.addAction(tr("Rotate 90°") + QStringLiteral("\tR"), [this] { rotate90(); });
    menu.addAction(tr("Flip horizontally") + QStringLiteral("\tH"),
                   [this] { flipHorizontal(); });
    if (system_) {
        menu.addSeparator();
        menu.addAction(tr("Click-through (turn off via tray menu)"), [this] {
            system_->setClickThrough(windowHandle(), true);
        });
    }
    menu.addSeparator();
    menu.addAction(tr("Close pin"), [this] { close(); });
    menu.exec(event->globalPos());
}

} // namespace pixora
