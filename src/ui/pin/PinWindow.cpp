#include "ui/pin/PinWindow.h"

#include "platform/interface/SystemIntegration.h"
#include "ui/Theme.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
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
constexpr int kMargin = theme::kShadowMargin; // 投影边距(兼缩放热区)
constexpr int kEdgeInside = 6;               // 图像内侧的缩放热区宽度
constexpr int kCloseSize = 22;
constexpr int kChipMs = 1100;                // 百分比小标签停留时长
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
    setAttribute(Qt::WA_TranslucentBackground); // 投影画在图像外圈
    setWindowTitle(tr("Pixora pin"));
    setMouseTracking(true); // 边缘热区光标反馈
    chipTimer_.setSingleShot(true);
    chipTimer_.setInterval(kChipMs);
    connect(&chipTimer_, &QTimer::timeout, this, [this] {
        chip_.clear();
        update();
    });
    rebuildDisplayCache();
    setContentGeometry(QRect(topLeftLogical, scaledSize()));
}

QPoint PinWindow::imageTopLeft() const {
    return pos() + QPoint(kMargin, kMargin);
}

QRect PinWindow::contentRect() const {
    return rect().adjusted(kMargin, kMargin, -kMargin, -kMargin);
}

void PinWindow::setContentGeometry(const QRect& globalContent) {
    setGeometry(globalContent.adjusted(-kMargin, -kMargin, kMargin, kMargin));
}

void PinWindow::resizeContent(const QSize& size) {
    resize(size + QSize(2 * kMargin, 2 * kMargin));
}

QRect PinWindow::closeButtonRect() const {
    const QRect c = contentRect();
    if (folded_ || c.width() < 80 || c.height() < 60) {
        return {}; // 太小或折叠时不放按钮,双击/Esc/菜单仍可关闭
    }
    return QRect(c.right() - 8 - kCloseSize + 1, c.top() + 8, kCloseSize, kCloseSize);
}

void PinWindow::flashChip(const QString& text) {
    chip_ = text;
    chipTimer_.start();
    update();
}

PinWindow::Edge PinWindow::edgeAt(const QPoint& pos) const {
    if (folded_) {
        return Edge::None; // 折叠小条不缩放
    }
    // 热区 = 图像内侧 kEdgeInside + 外圈整个投影边距
    const QRect c = contentRect();
    const bool left = pos.x() <= c.left() + kEdgeInside;
    const bool right = pos.x() >= c.right() - kEdgeInside;
    const bool top = pos.y() <= c.top() + kEdgeInside;
    const bool bottom = pos.y() >= c.bottom() - kEdgeInside;
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
    setContentGeometry(QRect(topLeft, sz));
    flashChip(QStringLiteral("%1%").arg(qRound(scale_ * 100)));
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
        resizeContent(QSize(std::clamp(scaledSize().width(), 80, 240), kFoldedHeight));
    } else {
        resizeContent(scaledSize());
    }
    update();
}

void PinWindow::applyScale(qreal scale) {
    scale_ = std::clamp(scale, kMinScale, kMaxScale);
    resizeContent(scaledSize());
    flashChip(QStringLiteral("%1%").arg(qRound(scale_ * 100)));
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
    const QRect c = contentRect();
    theme::paintShadow(painter, c, 0);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (folded_) {
        // 小条:图像顶部按宽度等比铺放,溢出裁剪
        painter.save();
        painter.setClipRect(c);
        const QSizeF s = display_.deviceIndependentSize();
        const qreal h = s.width() > 0 ? s.height() * c.width() / s.width() : c.height();
        painter.drawImage(QRectF(c.left(), c.top(), c.width(), h), display_);
        painter.fillRect(c, QColor(0, 0, 0, 60));
        painter.restore();
    } else {
        painter.drawImage(c, display_);
    }

    // 细边:常态淡白(深色图落在深色桌面上也有轮廓),悬停转主题蓝示意可操作
    painter.setPen(QPen(hovered_ ? theme::accent() : QColor(255, 255, 255, 46), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(c).adjusted(0.5, 0.5, -0.5, -0.5));

    painter.setRenderHint(QPainter::Antialiasing);
    const QRect closeRect = hovered_ ? closeButtonRect() : QRect();
    if (!closeRect.isNull()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::hudScrim());
        painter.drawEllipse(closeRect);
        painter.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap));
        const QRectF x = QRectF(closeRect).adjusted(7.5, 7.5, -7.5, -7.5);
        painter.drawLine(x.topLeft(), x.bottomRight());
        painter.drawLine(x.topRight(), x.bottomLeft());
    }

    const auto drawChip = [&painter](const QRect& r, const QString& text) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::hudScrim());
        painter.drawRoundedRect(r, theme::corner::chip, theme::corner::chip);
        painter.setPen(Qt::white);
        painter.drawText(r, Qt::AlignCenter, text);
    };
    const QFont f = theme::font(theme::fontsize::body);
    const QFontMetrics fm(f);
    painter.setFont(f);
    if (!badge_.isEmpty() && !folded_) {
        // 状态角标在右上;悬停出现关闭按钮时让到它左侧
        const int right = closeRect.isNull() ? c.right() - 7 : closeRect.left() - 6;
        const int w = fm.horizontalAdvance(badge_) + 14;
        drawChip(QRect(right - w + 1, c.top() + 8, w, fm.height() + 6), badge_);
    }
    if (!chip_.isEmpty() && !folded_) {
        const int w = fm.horizontalAdvance(chip_) + 14;
        const int h = fm.height() + 6;
        drawChip(QRect(c.right() - 7 - w + 1, c.bottom() - 7 - h + 1, w, h), chip_);
    }
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (const QRect closeRect = closeButtonRect();
        hovered_ && !closeRect.isNull() && closeRect.contains(event->pos())) {
        close();
        return;
    }
    resizeEdge_ = edgeAt(event->pos());
    if (resizeEdge_ != Edge::None) {
        resizing_ = true;
        baseGeometry_ = geometry().adjusted(kMargin, kMargin, -kMargin, -kMargin);
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
    // 悬停光标反馈:关闭按钮 → 手形,边缘 → 缩放箭头
    const QRect closeRect = closeButtonRect();
    if (!closeRect.isNull() && closeRect.contains(event->pos())) {
        setCursor(Qt::PointingHandCursor);
        return;
    }
    setCursor(cursorForEdge(edgeAt(event->pos())));
}

void PinWindow::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QWidget::enterEvent(event);
}

void PinWindow::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QWidget::leaveEvent(event);
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
        flashChip(tr("Opacity %1%").arg(qRound(windowOpacity() * 100)));
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
