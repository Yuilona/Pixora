#include "ui/scroll/ScrollCaptureBar.h"

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
    setStyleSheet(QStringLiteral(
        "QWidget { background: #2B2B2B; }"
        "QLabel { color: #DDD; font-size: 12px; padding: 0 6px; }"
        "QToolButton { color: #DDD; background: transparent; border: none;"
        "  padding: 4px 10px; font-size: 12px; }"
        "QToolButton:hover { background: #3D3D3D; }"
        "QToolButton:checked { background: #1E88E5; color: white; }"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    status_ = new QLabel(QStringLiteral("滚动目标窗口开始拼接…"), this);
    layout->addWidget(status_);

    if (autoModeAvailable) {
        autoBtn_ = new QToolButton(this);
        autoBtn_->setText(QStringLiteral("自动滚动"));
        autoBtn_->setCheckable(true);
        connect(autoBtn_, &QToolButton::toggled, this, &ScrollCaptureBar::autoToggled);
        layout->addWidget(autoBtn_);
    }

    auto* finishBtn = new QToolButton(this);
    finishBtn->setText(QStringLiteral("完成"));
    connect(finishBtn, &QToolButton::clicked, this, &ScrollCaptureBar::finishRequested);
    layout->addWidget(finishBtn);

    auto* cancelBtn = new QToolButton(this);
    cancelBtn->setText(QStringLiteral("取消"));
    connect(cancelBtn, &QToolButton::clicked, this, &ScrollCaptureBar::cancelRequested);
    layout->addWidget(cancelBtn);

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

} // namespace pixora
