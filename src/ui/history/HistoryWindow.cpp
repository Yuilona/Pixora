#include "ui/history/HistoryWindow.h"

#include "app/HistoryService.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
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
    setWindowTitle(QStringLiteral("截图历史 — Pixora"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(640, 460);

    list_ = new QListWidget(this);
    list_->setViewMode(QListView::IconMode);
    list_->setIconSize(QSize(kThumbW, kThumbH));
    list_->setResizeMode(QListView::Adjust);
    list_->setMovement(QListView::Static);
    list_->setSpacing(8);
    list_->setWordWrap(true);

    auto* buttons = new QHBoxLayout;
    auto addBtn = [this, buttons](const QString& text, auto onClicked) {
        auto* btn = new QPushButton(text, this);
        connect(btn, &QPushButton::clicked, this, onClicked);
        buttons->addWidget(btn);
        return btn;
    };
    addBtn(QStringLiteral("复制"), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            QGuiApplication::clipboard()->setImage(img);
        }
    });
    addBtn(QStringLiteral("贴图"), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            emit pinRequested(img);
        }
    });
    addBtn(QStringLiteral("另存…"), [this] {
        const QImage img = currentImage();
        if (!img.isNull()) {
            output_.saveWithDialog(img);
        }
    });
    addBtn(QStringLiteral("删除"), [this] {
        QString id;
        if (!currentImage(&id).isNull()) {
            history_.remove(id);
        }
    });
    buttons->addStretch();
    addBtn(QStringLiteral("清空历史"), [this] { history_.clear(); });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(list_, 1);
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
