#include "app/HistoryService.h"

#include "app/SettingsService.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kDefaultLimit = 20;
} // namespace

HistoryService::HistoryService(const SettingsService* settings,
                               const QString& dirOverride, QObject* parent)
    : QObject(parent), settings_(settings), dirOverride_(dirOverride) {
    // 启动加载清单;清单中文件已丢失的条目直接丢弃
    QFile file(dir() + QStringLiteral("/history.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.id = o[QStringLiteral("id")].toString();
        e.time = QDateTime::fromString(o[QStringLiteral("time")].toString(), Qt::ISODate);
        e.dpr = o[QStringLiteral("dpr")].toDouble(1.0);
        if (!e.id.isEmpty() && QFile::exists(pngPath(e.id))) {
            entries_.push_back(e);
        }
    }
}

QString HistoryService::dir() const {
    return dirOverride_.isEmpty()
               ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                     QStringLiteral("/history")
               : dirOverride_;
}

QString HistoryService::pngPath(const QString& id) const {
    return dir() + QStringLiteral("/%1.png").arg(id);
}

int HistoryService::limit() const {
    return settings_ ? settings_->historyLimit() : kDefaultLimit;
}

void HistoryService::record(const QImage& image) {
    if (image.isNull() || limit() <= 0) {
        return;
    }
    if (!QDir().mkpath(dir())) {
        spdlog::warn("history dir unavailable: {}", dir().toStdString());
        return;
    }
    Entry e;
    e.time = QDateTime::currentDateTime();
    e.dpr = image.devicePixelRatio();
    e.id = QStringLiteral("%1_%2").arg(
        e.time.toString(QStringLiteral("yyyyMMdd_HHmmss")),
        QString::number(seq_++));
    if (!image.save(pngPath(e.id))) {
        spdlog::warn("history save failed: {}", e.id.toStdString());
        return;
    }
    entries_.insert(entries_.begin(), e);
    prune();
    saveManifest();
    emit changed();
}

QImage HistoryService::load(const Entry& entry) const {
    QImage image(pngPath(entry.id));
    if (!image.isNull()) {
        image.setDevicePixelRatio(entry.dpr);
    }
    return image;
}

void HistoryService::remove(const QString& id) {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&id](const Entry& e) { return e.id == id; });
    if (it == entries_.end()) {
        return;
    }
    QFile::remove(pngPath(id));
    entries_.erase(it);
    saveManifest();
    emit changed();
}

void HistoryService::clear() {
    for (const Entry& e : entries_) {
        QFile::remove(pngPath(e.id));
    }
    entries_.clear();
    saveManifest();
    emit changed();
}

void HistoryService::prune() {
    const size_t cap = static_cast<size_t>(limit());
    while (entries_.size() > cap) {
        QFile::remove(pngPath(entries_.back().id));
        entries_.pop_back();
    }
}

void HistoryService::saveManifest() {
    QJsonArray arr;
    for (const Entry& e : entries_) {
        QJsonObject o;
        o[QStringLiteral("id")] = e.id;
        o[QStringLiteral("time")] = e.time.toString(Qt::ISODate);
        o[QStringLiteral("dpr")] = e.dpr;
        arr.append(o);
    }
    QSaveFile file(dir() + QStringLiteral("/history.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

} // namespace pixora
