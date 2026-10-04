#pragma once

#include "app/OutputService.h"

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QScrollArea;

namespace pixora {

class HistoryService;
class SettingsService;

// 截图历史窗:按日期分组(今天/昨天/具体日期)的卡片网格,新→旧。
// 卡片悬停浮出 复制/贴图/另存/删除;双击卡片 = 复制;复制后窗内短暂提示。
// 贴图出口经信号回 App 层(贴图归 PinService 管)。
class HistoryWindow : public QWidget {
    Q_OBJECT
public:
    HistoryWindow(HistoryService& history, const SettingsService* settings);

signals:
    void pinRequested(const QImage& image);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Thumb {
        QPixmap pixmap;
        QSize logicalSize;
    };

    void reload();
    Thumb thumbFor(const QString& id);
    QImage imageFor(const QString& id) const;
    void copyImage(const QString& id);
    void flash(const QString& text); // 窗内底部短暂提示
    void placeFlash();

    HistoryService& history_;
    OutputService output_;
    QScrollArea* scroll_ = nullptr;
    QLabel* emptyHint_ = nullptr; // 无历史时的占位提示
    QLabel* countLabel_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    QLabel* flash_ = nullptr;
    QTimer flashTimer_;
    QHash<QString, Thumb> thumbs_; // 缩略图缓存:删除/清空时不必重读全部原图
};

} // namespace pixora
