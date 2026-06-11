#pragma once

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>

#include <vector>

namespace pixora {

class SettingsService;

// 截图历史:每次截图出口(复制/另存/贴图,含长截图)自动留底——
// <AppData>/history/<id>.png + history.json 清单(时间/DPR)。
// 超出保留上限(设置项,0=关闭)淘汰最旧;托盘"历史…"窗口消费。
class HistoryService : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString id; // 文件名主干,时间戳派生
        QDateTime time;
        qreal dpr = 1.0;
    };

    // dirOverride 供测试注入;为空用 <AppData>/history
    explicit HistoryService(const SettingsService* settings,
                            const QString& dirOverride = {}, QObject* parent = nullptr);

    void record(const QImage& image);
    const std::vector<Entry>& entries() const { return entries_; } // 新→旧
    QImage load(const Entry& entry) const;
    void remove(const QString& id);
    void clear();

signals:
    void changed();

private:
    QString dir() const;
    QString pngPath(const QString& id) const;
    int limit() const;
    void saveManifest();
    void prune();

    const SettingsService* settings_;
    QString dirOverride_;
    std::vector<Entry> entries_;
    int seq_ = 0; // 同秒多张时保证 id 唯一
};

} // namespace pixora
