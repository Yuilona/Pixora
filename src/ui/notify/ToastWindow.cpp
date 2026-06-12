#include "ui/notify/ToastWindow.h"

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
constexpr int kFadeMs = 220;
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
    connect(&hideTimer_, &QTimer::timeout, this, &ToastWindow::startFadeOut);

    fade_.setTargetObject(this);
    fade_.setPropertyName("windowOpacity");
    fade_.setDuration(kFadeMs);
    fade_.setStartValue(1.0);
    fade_.setEndValue(0.0);
    connect(&fade_, &QPropertyAnimation::finished, this, &QWidget::hide);
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
    move(avail.right() - w - kMargin, avail.bottom() - h - kMargin);

    fade_.stop();
    setWindowOpacity(1.0);
    show();
    raise();
    hideTimer_.start();
    update();
}

void ToastWindow::startFadeOut() {
    if (isVisible()) {
        fade_.start();
    }
}

void ToastWindow::mousePressEvent(QMouseEvent* /*event*/) {
    hideTimer_.stop();
    fade_.stop();
    hide();
}

void ToastWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath card;
    card.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    p.fillPath(card, QColor(0x2B, 0x2B, 0x2B, 247));
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawPath(card);

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
    p.setPen(QColor(Qt::white));
    p.drawText(QRect(textX, kPadding, textW, titleFm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, title_);
    p.setFont(bodyFont);
    p.setPen(QColor(0xDD, 0xDD, 0xDD));
    p.drawText(bodyArea, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message_);
}

} // namespace pixora
