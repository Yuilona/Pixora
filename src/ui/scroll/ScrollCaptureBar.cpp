#include "ui/scroll/ScrollCaptureBar.h"

#include "ui/InstantTip.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QToolButton>

#include <algorithm>

namespace pixora {

namespace {

constexpr int kStatusWidth = 220; // 提示区定宽(可折两行),状态切换不跳宽
constexpr int kDotSize = 8;

QColor toneColor(ScrollCaptureBar::Tone tone) {
    switch (tone) {
    case ScrollCaptureBar::Tone::Good:
        return theme::success();
    case ScrollCaptureBar::Tone::Warning:
        return theme::warning();
    case ScrollCaptureBar::Tone::Neutral:
        break;
    }
    return theme::textFaint();
}

QWidget* separator(QWidget* parent) {
    auto* sep = new QWidget(parent);
    sep->setFixedSize(1, 18);
    const QColor line = theme::hairline();
    sep->setStyleSheet(QStringLiteral("background: rgba(%1,%2,%3,%4);")
                           .arg(line.red())
                           .arg(line.green())
                           .arg(line.blue())
                           .arg(line.alpha()));
    return sep;
}

} // namespace

ScrollCaptureBar::ScrollCaptureBar(const QRect& regionGlobal, const QRect& virtualBounds,
                                   bool autoModeAvailable) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground); // 配合 paintFloatingCard 圆角卡片 + 投影
    setStyleSheet(theme::chromeStyleSheet(theme::fontsize::body, /*padV=*/5, /*padH=*/7));

    auto* layout = new QHBoxLayout(this);
    constexpr int m = theme::kShadowMargin; // 四周留出投影边距
    layout->setContentsMargins(m + 12, m + 4, m + 6, m + 4);
    layout->setSpacing(2);

    dot_ = new QWidget(this);
    dot_->setFixedSize(kDotSize, kDotSize);
    layout->addWidget(dot_);
    layout->addSpacing(8);

    height_ = new QLabel(this);
    height_->setStyleSheet(QStringLiteral("color: %1; font-size: 15px; font-weight: 600;"
                                          " padding: 0;")
                               .arg(theme::text().name()));
    height_->setMinimumWidth(76); // 位数增长(999 → 12,345)时不推挤整条
    layout->addWidget(height_);
    layout->addSpacing(6);

    status_ = new QLabel(this);
    status_->setFixedWidth(kStatusWidth);
    status_->setWordWrap(true);
    status_->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; padding: 0 4px;")
                               .arg(theme::textDim().name())
                               .arg(theme::fontsize::caption));
    layout->addWidget(status_);

    auto* tip = new InstantTip(this);

    // "自动滚动"是模式开关,无公认图形,保留文字最清楚
    if (autoModeAvailable) {
        layout->addSpacing(4);
        layout->addWidget(separator(this));
        layout->addSpacing(4);
        autoBtn_ = new QToolButton(this);
        autoBtn_->setText(tr("Auto-scroll"));
        autoBtn_->setCheckable(true);
        connect(autoBtn_, &QToolButton::toggled, this, &ScrollCaptureBar::autoToggled);
        layout->addWidget(autoBtn_);
    }
    layout->addSpacing(4);
    layout->addWidget(separator(this));
    layout->addSpacing(4);

    auto addButton = [this, layout, tip](const QIcon& icon, const QString& tooltip,
                                         auto signal) {
        auto* btn = new QToolButton(this);
        btn->setIcon(icon);
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tooltip);
        btn->installEventFilter(tip);
        connect(btn, &QToolButton::clicked, this, signal);
        layout->addWidget(btn);
        return btn;
    };
    // 撤销最后一段:预览里看到错位时回退,滚回去重拼
    undoBtn_ = addButton(icons::undoIcon(), tr("Undo last segment"),
                         &ScrollCaptureBar::undoRequested);
    undoBtn_->setEnabled(false);
    layout->addSpacing(4);
    layout->addWidget(separator(this));
    layout->addSpacing(4);
    addButton(icons::pinIcon(), tr("Finish and pin"),
              &ScrollCaptureBar::finishPinRequested);
    addButton(icons::saveIcon(), tr("Finish and save"),
              &ScrollCaptureBar::finishSaveRequested);
    addButton(icons::cancelIcon(), tr("Cancel"), &ScrollCaptureBar::cancelRequested);
    layout->addSpacing(4);
    QToolButton* copyBtn = addButton(icons::primaryConfirmIcon(), tr("Finish and copy (F1)"),
                                     &ScrollCaptureBar::finishRequested);
    copyBtn->setObjectName(QStringLiteral("primary"));
    copyBtn->setText(tr("Copy"));
    copyBtn->setIconSize(QSize(16, 16));
    copyBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    setProgress(0);
    setStatus(tr("Scroll the target window to start stitching..."));

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

void ScrollCaptureBar::setProgress(int logicalHeight) {
    height_->setText(QStringLiteral("%1 px").arg(QLocale().toString(logicalHeight)));
}

void ScrollCaptureBar::setStatus(const QString& text, Tone tone) {
    status_->setText(text);
    dot_->setStyleSheet(QStringLiteral("background: %1; border-radius: %2px;")
                            .arg(toneColor(tone).name())
                            .arg(kDotSize / 2));
}

void ScrollCaptureBar::setUndoEnabled(bool enabled) {
    undoBtn_->setEnabled(enabled);
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
