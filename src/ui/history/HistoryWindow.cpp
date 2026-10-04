#include "ui/history/HistoryWindow.h"

#include "app/HistoryService.h"
#include "ui/Theme.h"
#include "ui/ToolIcons.h"
#include "ui/history/FlowLayout.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace pixora {

namespace {

constexpr QSize kCardSize(200, 168);
constexpr QRect kThumbRect(8, 8, 184, 116); // 卡片内缩略图区
constexpr int kFlashMs = 1400;

// 卡片不是 QObject:文案经 QT_TRANSLATE_NOOP 标记到 HistoryWindow 上下文,此处取译文
QString trHistory(const char* text) {
    return QCoreApplication::translate("pixora::HistoryWindow", text);
}

// 历史卡片:白卡 + 缩略图(等比居中于浅灰底)+ 时间/尺寸;
// 悬停描边转主题蓝,并在缩略图底部浮出深色操作条。
class HistoryCard : public QWidget {
public:
    struct Actions {
        std::function<void()> copy;
        std::function<void()> pin;
        std::function<void()> save;
        std::function<void()> remove;
    };

    HistoryCard(const QPixmap& thumb, const QString& time, const QString& size,
                const Actions& actions, QWidget* parent)
        : QWidget(parent), thumb_(thumb), time_(time), size_(size), actions_(actions) {
        setFixedSize(kCardSize);
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setToolTip(trHistory(QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Double-click to copy")));

        actionBar_ = new QWidget(this);
        actionBar_->setAttribute(Qt::WA_StyledBackground);
        actionBar_->setStyleSheet(QStringLiteral(
            "QWidget { background: rgba(20, 22, 26, 205); border-radius: 8px; }"
            "QToolButton { background: transparent; border: none; border-radius: 6px;"
            "  padding: 4px; }"
            "QToolButton:hover { background: rgba(255, 255, 255, 40); }"));
        auto* bar = new QHBoxLayout(actionBar_);
        bar->setContentsMargins(4, 3, 4, 3);
        bar->setSpacing(2);
        const auto addAction = [this, bar](const char* icon, const char* tip,
                                           const std::function<void()>& fn) {
            auto* btn = new QToolButton(actionBar_);
            btn->setIcon(icons::onImageIcon(icon));
            btn->setIconSize(QSize(18, 18));
            btn->setToolTip(trHistory(tip));
            btn->setCursor(Qt::ArrowCursor);
            connect(btn, &QToolButton::clicked, this, fn);
            bar->addWidget(btn);
        };
        addAction("copy", QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Copy"), actions_.copy);
        addAction("pin", QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Pin"), actions_.pin);
        addAction("save", QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Save as..."), actions_.save);
        addAction("delete", QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Delete"), actions_.remove);
        actionBar_->adjustSize();
        actionBar_->move(kThumbRect.center().x() - actionBar_->width() / 2,
                         kThumbRect.bottom() - actionBar_->height() - 6);
        actionBar_->hide();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool hovered = underMouse();
        p.setPen(QPen(hovered ? theme::accent() : QColor(0xE3, 0xE6, 0xEB),
                      hovered ? 1.5 : 1.0));
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75),
                          theme::corner::card, theme::corner::card);

        QPainterPath clip;
        clip.addRoundedRect(QRectF(kThumbRect), theme::corner::control,
                            theme::corner::control);
        p.save();
        p.setClipPath(clip);
        p.fillRect(kThumbRect, QColor(0xEE, 0xF0, 0xF4));
        const QSize s = thumb_.deviceIndependentSize().toSize();
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(QRect(kThumbRect.center() - QPoint(s.width() / 2, s.height() / 2), s),
                     thumb_);
        p.restore();

        const QRect caption(12, kThumbRect.bottom() + 6, width() - 24,
                            height() - kThumbRect.bottom() - 12);
        p.setFont(theme::font(theme::fontsize::label, /*bold=*/true));
        p.setPen(QColor(0x1F, 0x23, 0x29));
        p.drawText(caption, Qt::AlignLeft | Qt::AlignVCenter, time_);
        p.setFont(theme::font(theme::fontsize::body));
        p.setPen(QColor(0x8B, 0x91, 0x9C));
        p.drawText(caption, Qt::AlignRight | Qt::AlignVCenter, size_);
    }

    void enterEvent(QEnterEvent* event) override {
        actionBar_->show();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        actionBar_->hide();
        QWidget::leaveEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* /*event*/) override { actions_.copy(); }

private:
    QPixmap thumb_;
    QString time_;
    QString size_;
    Actions actions_;
    QWidget* actionBar_ = nullptr;
};

QString groupTitle(const QDate& date) {
    const QDate today = QDate::currentDate();
    if (date == today) {
        return trHistory(QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Today"));
    }
    if (date == today.addDays(-1)) {
        return trHistory(QT_TRANSLATE_NOOP("pixora::HistoryWindow", "Yesterday"));
    }
    return QLocale().toString(date, QLocale::LongFormat);
}

} // namespace

HistoryWindow::HistoryWindow(HistoryService& history, const SettingsService* settings)
    : history_(history), output_(settings) {
    setWindowTitle(tr("History - Pixora"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(700, 540);

    // 普通 QWidget 顶层窗不吃全局 QDialog 底色规则,用调色板对齐
    QPalette pal = palette();
    pal.setColor(QPalette::Window, theme::lightWindowBg());
    setPalette(pal);
    setAutoFillBackground(true);

    countLabel_ = new QLabel(this);
    countLabel_->setStyleSheet(QStringLiteral("color:#8B919C;"));
    clearButton_ = new QPushButton(icons::lightIcon("delete"), tr("Clear history"), this);
    clearButton_->setStyleSheet(theme::dangerButtonStyle()); // 破坏性操作,红字弱底
    connect(clearButton_, &QPushButton::clicked, this, [this] { history_.clear(); });

    auto* top = new QHBoxLayout;
    top->addWidget(countLabel_);
    top->addStretch();
    top->addWidget(clearButton_);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    emptyHint_ = new QLabel(
        tr("No screenshots yet\n\nEvery capture you copy, save or pin is kept "
           "here automatically"),
        this);
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet(QStringLiteral("color:#9AA3B0; font-size:13px;"));
    emptyHint_->hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 10, 12);
    layout->setSpacing(10);
    layout->addLayout(top);
    layout->addWidget(scroll_, 1);
    layout->addWidget(emptyHint_, 1);

    flash_ = new QLabel(this);
    flash_->setStyleSheet(QStringLiteral(
        "background: rgba(20, 22, 26, 225); color: white; border-radius: 8px;"
        " padding: 7px 16px; font-size: 13px;"));
    flash_->hide();
    flashTimer_.setSingleShot(true);
    connect(&flashTimer_, &QTimer::timeout, flash_, &QWidget::hide);

    // 排队重载:删除动作由卡片自己的按钮发起,同步重建会在点击处理中途删掉该卡片
    connect(&history_, &HistoryService::changed, this, &HistoryWindow::reload,
            Qt::QueuedConnection);
    reload();
}

HistoryWindow::Thumb HistoryWindow::thumbFor(const QString& id) {
    if (const auto it = thumbs_.constFind(id); it != thumbs_.constEnd()) {
        return *it;
    }
    Thumb thumb;
    const QImage img = imageFor(id);
    if (!img.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QImage scaled = img.scaled(kThumbRect.size() * dpr, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        thumb.pixmap = QPixmap::fromImage(scaled);
        thumb.logicalSize = img.deviceIndependentSize().toSize();
    }
    thumbs_.insert(id, thumb);
    return thumb;
}

QImage HistoryWindow::imageFor(const QString& id) const {
    for (const HistoryService::Entry& e : history_.entries()) {
        if (e.id == id) {
            return history_.load(e);
        }
    }
    return {};
}

void HistoryWindow::reload() {
    const int scrollPos = scroll_->verticalScrollBar()->value();

    auto* content = new QWidget;
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 8, 8);
    column->setSpacing(8);

    QDate currentDate;
    FlowLayout* flow = nullptr;
    int count = 0;
    for (const HistoryService::Entry& e : history_.entries()) {
        const Thumb thumb = thumbFor(e.id);
        if (thumb.pixmap.isNull()) {
            continue;
        }
        if (!flow || e.time.date() != currentDate) {
            currentDate = e.time.date();
            auto* header = new QLabel(groupTitle(currentDate), content);
            header->setStyleSheet(QStringLiteral(
                "color:#1F2329; font-size:14px; font-weight:600; padding-top:6px;"));
            column->addWidget(header);
            auto* grid = new QWidget(content);
            flow = new FlowLayout(grid);
            column->addWidget(grid);
        }
        const QString id = e.id;
        HistoryCard::Actions actions;
        actions.copy = [this, id] { copyImage(id); };
        actions.pin = [this, id] {
            const QImage img = imageFor(id);
            if (!img.isNull()) {
                emit pinRequested(img);
            }
        };
        actions.save = [this, id] {
            const QImage img = imageFor(id);
            if (!img.isNull()) {
                output_.saveWithDialog(img);
            }
        };
        actions.remove = [this, id] {
            thumbs_.remove(id);
            history_.remove(id);
        };
        flow->addWidget(new HistoryCard(
            thumb.pixmap, e.time.toString(QStringLiteral("HH:mm:ss")),
            QStringLiteral("%1 × %2")
                .arg(thumb.logicalSize.width())
                .arg(thumb.logicalSize.height()),
            actions, content));
        ++count;
    }
    column->addStretch();
    scroll_->setWidget(content); // 旧内容随之销毁
    scroll_->verticalScrollBar()->setValue(scrollPos);

    const bool empty = count == 0;
    scroll_->setVisible(!empty);
    emptyHint_->setVisible(empty);
    clearButton_->setEnabled(!empty);
    countLabel_->setText(empty ? QString() : tr("%1 screenshots").arg(count));
}

void HistoryWindow::copyImage(const QString& id) {
    const QImage img = imageFor(id);
    if (img.isNull()) {
        return;
    }
    QGuiApplication::clipboard()->setImage(img);
    flash(tr("Copied to clipboard"));
}

void HistoryWindow::flash(const QString& text) {
    flash_->setText(text);
    flash_->adjustSize();
    placeFlash();
    flash_->show();
    flash_->raise();
    flashTimer_.start(kFlashMs);
}

void HistoryWindow::placeFlash() {
    flash_->move((width() - flash_->width()) / 2, height() - flash_->height() - 24);
}

void HistoryWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    placeFlash();
}

} // namespace pixora
