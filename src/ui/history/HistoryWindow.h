#pragma once

#include "app/OutputService.h"

#include <QImage>
#include <QWidget>

class QLabel;
class QListWidget;

namespace pixora {

class HistoryService;
class SettingsService;

// 截图历史窗:缩略图列表(新→旧)+ 复制/贴图/另存/删除/清空。
// 双击条目等同复制。贴图出口经信号回 App 层(贴图归 PinService 管)。
class HistoryWindow : public QWidget {
    Q_OBJECT
public:
    HistoryWindow(HistoryService& history, const SettingsService* settings);

signals:
    void pinRequested(const QImage& image);

private:
    void reload();
    QImage currentImage(QString* id = nullptr) const;

    HistoryService& history_;
    OutputService output_;
    QListWidget* list_ = nullptr;
    QLabel* emptyHint_ = nullptr; // 无历史时的占位提示
};

} // namespace pixora
