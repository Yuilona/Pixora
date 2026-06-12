#include "ui/history/HistoryWindow.h"

#include "app/HistoryService.h"
#include "ui/Theme.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace pixora {

namespace {
constexpr int kThumbW = 180;
constexpr int kThumbH = 110;
} // namespace

HistoryWindow::HistoryWindow(HistoryService& history, const SettingsService* settings)
    : history_(history), output_(settings) {
    setWindowTitle(tr("History - Pixora"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(660, 480);

    // 普通 QWidget 顶层窗不吃全局 QDialog 底色规则,用调色板对齐
    QPalette pal = palette();
    pal.setColor(QPalette::Window, theme::lightWindowBg());
    setPalette(pal);
    setAutoFillBackground(true);

    list_ = new QListWidget(this);
    list_->setViewMode(QListView::IconMode);
    list_->setIconSize(QSize(kThumbW, kThumbH));
    list_->setResizeMode(QListView::Adjust);
    list_->setMovement(QListView::Static);
    list_->setSpacing(10);
    list_->setWordWrap(true);

    emptyHint_ = new QLabel(
        tr("No screenshots yet\n\nEvery capture you copy, save or pin is kept "
           "here automatically"),
        this);
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet(QStringLiteral("color:#9AA3B0; font-size:13px;"));
    emptyHint_->hide();

    auto* buttons = new QHBoxLayout;
    auto addBtn = [this, buttons](const QString& text, auto onClicked) {
        auto* btn = new QPushButton(text, this);
        connect(btn, &QPushButton::clicked, this, onClicked);
        buttons->addWidget(btn);
        return btn;
    };
    auto* copyBtn = addBtn(tr("Copy"), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            QGuiApplication::clipboard()->setImage(img);
        }
    });
    copyBtn->setStyleSheet(theme::primaryButtonStyle()); // 最高频出口,视觉主按钮
    auto* pinBtn = addBtn(tr("Pin"), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            emit pinRequested(img);
        }
    });
    auto* saveBtn = addBtn(tr("Save as..."), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            output_.saveWithDialog(img);
        }
    });
    auto* deleteBtn = addBtn(tr("Delete"), [this] {
        QString id;
        if (!currentImage(&id).isNull()) {
            history_.remove(id);
        }
    });
    buttons->addStretch();
    auto* clearBtn = addBtn(tr("Clear history"), [this] { history_.clear(); });
    clearBtn->setStyleSheet(theme::dangerButtonStyle()); // 破坏性操作,红字弱底

    // 无选中条目时单条目操作不可点;无任何历史时清空也不可点
    selectionButtons_ = {copyBtn, pinBtn, saveBtn, deleteBtn};
    clearButton_ = clearBtn;
    connect(list_, &QListWidget::currentItemChanged, this,
            [this] { updateButtonStates(); });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);
    layout->addWidget(list_, 1);
    layout->addWidget(emptyHint_, 1);
    layout->addLayout(buttons);

    // 双击 = 复制(最高频出口)
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        const QImage img = currentImage();
        if (!img.isNull()) {
            QGuiApplication::clipboard()->setImage(img);
        }
    });
    connect(&history_, &HistoryService::changed, this, &HistoryWindow::reload);
    reload();
}

void HistoryWindow::reload() {
    list_->clear();
    for (const HistoryService::Entry& e : history_.entries()) {
        const QImage img = history_.load(e);
        if (img.isNull()) {
            continue;
        }
        auto* item = new QListWidgetItem(list_);
        item->setIcon(QIcon(QPixmap::fromImage(img.scaled(
            kThumbW, kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        item->setText(QStringLiteral("%1\n%2 × %3")
                          .arg(e.time.toString(QStringLiteral("MM-dd HH:mm:ss")))
                          .arg(qRound(img.width() / e.dpr))
                          .arg(qRound(img.height() / e.dpr)));
        item->setData(Qt::UserRole, e.id);
        item->setSizeHint(QSize(kThumbW + 16, kThumbH + 44));
    }
    const bool empty = list_->count() == 0;
    list_->setVisible(!empty);
    emptyHint_->setVisible(empty);
    updateButtonStates();
}

void HistoryWindow::updateButtonStates() {
    const bool hasSelection = list_->currentItem() != nullptr;
    for (QPushButton* btn : selectionButtons_) {
        btn->setEnabled(hasSelection);
    }
    clearButton_->setEnabled(list_->count() > 0);
}

QImage HistoryWindow::currentImage(QString* id) const {
    QListWidgetItem* item = list_->currentItem();
    if (!item) {
        return {};
    }
    const QString itemId = item->data(Qt::UserRole).toString();
    if (id) {
        *id = itemId;
    }
    for (const HistoryService::Entry& e : history_.entries()) {
        if (e.id == itemId) {
            return history_.load(e);
        }
    }
    return {};
}

} // namespace pixora
