#include "ui/notify/ToastWindow.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kMargin = 16;       // 距屏幕角
constexpr int kPadding = 14;      // 卡片内边距
constexpr int kLogoSize = 28;
constexpr int kGap = 12;          // logo 与文字间距
constexpr int kMaxTextWidth = 320;
constexpr int kShowMs = 3500;
constexpr int kEnterMs = 300;     // 滑入时长
constexpr int kExitMs = 240;      // 滑出时长
constexpr int kExitSlide = 32;    // 滑出位移(不必出屏,淡出已接管)
} // namespace

ToastWindow::ToastWindow() {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    logo_ = QPixmap(QStringLiteral(":/icons/pixora-256.png"))
                .scaled(kLogoSize * 2, kLogoSize * 2, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
    logo_.setDevicePixelRatio(2.0);

    hideTimer_.setSingleShot(true);
    hideTimer_.setInterval(kShowMs);
    connect(&hideTimer_, &QTimer::timeout, this, &ToastWindow::startExit);

    // 入场:从屏幕右缘滑入 + 淡入(OutCubic 先快后缓,落位干脆)
    enterPos_ = new QPropertyAnimation(this, "pos", this);
    enterPos_->setDuration(kEnterMs);
    enterPos_->setEasingCurve(QEasingCurve::OutCubic);
    enterOpacity_ = new QPropertyAnimation(this, "windowOpacity", this);
    enterOpacity_->setDuration(kEnterMs * 2 / 3);
    enterOpacity_->setStartValue(0.0);
    enterOpacity_->setEndValue(1.0);
    enter_ = new QParallelAnimationGroup(this);
    enter_->addAnimation(enterPos_);
    enter_->addAnimation(enterOpacity_);

    // 退场:轻微右滑 + 淡出
    exitPos_ = new QPropertyAnimation(this, "pos", this);
    exitPos_->setDuration(kExitMs);
    exitPos_->setEasingCurve(QEasingCurve::InCubic);
    exitOpacity_ = new QPropertyAnimation(this, "windowOpacity", this);
    exitOpacity_->setDuration(kExitMs);
    exitOpacity_->setStartValue(1.0);
    exitOpacity_->setEndValue(0.0);
    exit_ = new QParallelAnimationGroup(this);
    exit_->addAnimation(exitPos_);
    exit_->addAnimation(exitOpacity_);
    connect(exit_, &QParallelAnimationGroup::finished, this, &QWidget::hide);
}

void ToastWindow::popup(const QString& title, const QString& message) {
    title_ = title;
    message_ = message;

    QFont titleFont = font();
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    QFont bodyFont = font();
    bodyFont.setPixelSize(12);

    const QFontMetrics titleFm(titleFont);
    const QFontMetrics bodyFm(bodyFont);
    const QRect bodyRect = bodyFm.boundingRect(
        QRect(0, 0, kMaxTextWidth, 1000), Qt::TextWordWrap, message_);
    const int textWidth = std::clamp(
        std::max(titleFm.horizontalAdvance(title_), bodyRect.width()), 120,
        kMaxTextWidth);
    const int textHeight = titleFm.height() + 4 + bodyRect.height();

    const int w = kPadding * 2 + kLogoSize + kGap + textWidth;
    const int h = kPadding * 2 + std::max(kLogoSize, textHeight);
    resize(w, h);

    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect avail = screen->availableGeometry();
    const QPoint target(avail.right() - w - kMargin, avail.bottom() - h - kMargin);

    enter_->stop();
    exit_->stop();
    if (isVisible()) {
        // 已在屏上:直接换内容落位,不重播入场(连发通知不抖动)
        move(target);
        setWindowOpacity(1.0);
        update();
    } else {
        const QPoint offscreen(avail.right() + 8, target.y());
        setWindowOpacity(0.0);
        move(offscreen);
        show();
        raise();
        enterPos_->setStartValue(offscreen);
        enterPos_->setEndValue(target);
        enter_->start();
    }
    hideTimer_.start();
}

void ToastWindow::startExit() {
    if (!isVisible()) {
        return;
    }
    exitPos_->setStartValue(pos());
    exitPos_->setEndValue(pos() + QPoint(kExitSlide, 0));
    exit_->start();
}

void ToastWindow::mousePressEvent(QMouseEvent* /*event*/) {
    hideTimer_.stop();
    enter_->stop();
    exit_->stop();
    hide();
}

void ToastWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    theme::paintCard(p, rect(), 10, /*bgAlpha=*/247);

    const int logoY = (height() - kLogoSize) / 2;
    p.drawPixmap(kPadding, logoY, kLogoSize, kLogoSize, logo_);

    QFont titleFont = font();
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    QFont bodyFont = font();
    bodyFont.setPixelSize(12);
    const QFontMetrics titleFm(titleFont);

    const int textX = kPadding + kLogoSize + kGap;
    const int textW = width() - textX - kPadding;
    const QRect bodyArea(textX, kPadding + titleFm.height() + 4, textW,
                         height() - kPadding * 2 - titleFm.height() - 4);

    p.setFont(titleFont);
    p.setPen(theme::text());
    p.drawText(QRect(textX, kPadding, textW, titleFm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setFont(bodyFont);
    p.setPen(theme::textDim());
    p.drawText(bodyArea, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message_);
}

} // namespace pixora
