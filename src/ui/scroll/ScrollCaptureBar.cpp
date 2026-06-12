#include "ui/scroll/ScrollCaptureBar.h"

#include "ui/InstantTip.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

#include <algorithm>

namespace pixora {

ScrollCaptureBar::ScrollCaptureBar(const QRect& regionGlobal, const QRect& virtualBounds,
                                   bool autoModeAvailable) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground); // 配合 paintCard 圆角卡片底
    setStyleSheet(theme::chromeStyleSheet(/*fontPx=*/12, /*padV=*/4, /*padH=*/10));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    status_ = new QLabel(QStringLiteral("滚动目标窗口开始拼接…"), this);
    layout->addWidget(status_);

    auto* tip = new InstantTip(this);

    // "自动滚动"是模式开关,无公认图形,保留文字最清楚
    if (autoModeAvailable) {
        autoBtn_ = new QToolButton(this);
        autoBtn_->setText(QStringLiteral("自动滚动"));
        autoBtn_->setCheckable(true);
        connect(autoBtn_, &QToolButton::toggled, this, &ScrollCaptureBar::autoToggled);
        layout->addWidget(autoBtn_);
    }

    auto addButton = [this, layout, tip](const QIcon& icon, const QString& tooltip,
                                         auto signal) {
        auto* btn = new QToolButton(this);
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->installEventFilter(tip);
        connect(btn, &QToolButton::clicked, this, signal);
        layout->addWidget(btn);
    };
    addButton(icons::pinIcon(), QStringLiteral("完成并贴图"),
              &ScrollCaptureBar::finishPinRequested);
    addButton(icons::saveIcon(), QStringLiteral("完成并另存"),
              &ScrollCaptureBar::finishSaveRequested);
    addButton(icons::cancelIcon(), QStringLiteral("取消"),
              &ScrollCaptureBar::cancelRequested);
    addButton(icons::confirmIcon(), QStringLiteral("完成并复制 (F1)"),
              &ScrollCaptureBar::finishRequested);

    adjustSize();
    QPoint pos(regionGlobal.left(), regionGlobal.bottom() + 12);
    if (pos.y() + height() > virtualBounds.bottom()) {
        pos.setY(regionGlobal.top() - height() - 12);
    }
    pos.setX(std::clamp(pos.x(), virtualBounds.left(),
                        virtualBounds.right() - width() + 1));
    move(pos);
}

void ScrollCaptureBar::setStatus(const QString& text) {
    status_->setText(text);
    adjustSize();
}

void ScrollCaptureBar::setAutoChecked(bool checked) {
    if (autoBtn_) {
        autoBtn_->setChecked(checked);
    }
}

void ScrollCaptureBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    theme::paintCard(p, rect(), 8);
}

} // namespace pixora
