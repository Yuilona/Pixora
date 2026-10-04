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
    setAttribute(Qt::WA_TranslucentBackground); // 配合 paintFloatingCard 圆角卡片 + 投影
    setStyleSheet(theme::chromeStyleSheet(theme::fontsize::body, /*padV=*/4, /*padH=*/10));

    auto* layout = new QHBoxLayout(this);
    constexpr int m = theme::kShadowMargin; // 四周留出投影边距
    layout->setContentsMargins(m + 6, m + 4, m + 6, m + 4);
    layout->setSpacing(2);

    status_ = new QLabel(tr("Scroll the target window to start stitching..."), this);
    layout->addWidget(status_);

    auto* tip = new InstantTip(this);

    // "自动滚动"是模式开关,无公认图形,保留文字最清楚
    if (autoModeAvailable) {
        autoBtn_ = new QToolButton(this);
        autoBtn_->setText(tr("Auto-scroll"));
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
    addButton(icons::pinIcon(), tr("Finish and pin"),
              &ScrollCaptureBar::finishPinRequested);
    addButton(icons::saveIcon(), tr("Finish and save"),
              &ScrollCaptureBar::finishSaveRequested);
    addButton(icons::cancelIcon(), tr("Cancel"),
              &ScrollCaptureBar::cancelRequested);
    addButton(icons::confirmIcon(), tr("Finish and copy (F1)"),
              &ScrollCaptureBar::finishRequested);

    adjustSize();
    // 按卡片对齐捕获区域。卡片与区域留 12px 间距,投影向上只伸 blur-dy(7px),
    // 不会落进捕获区域污染拼接帧
    const int cardW = width() - 2 * m;
    const int cardH = height() - 2 * m;
    QPoint pos(regionGlobal.left(), regionGlobal.bottom() + 12);
    if (pos.y() + cardH > virtualBounds.bottom()) {
        pos.setY(regionGlobal.top() - cardH - 12);
    }
    pos.setX(std::clamp(pos.x(), virtualBounds.left(),
                        virtualBounds.right() - cardW + 1));
    move(pos - QPoint(m, m));
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
    theme::paintFloatingCard(p, rect(), theme::corner::card);
}

} // namespace pixora
