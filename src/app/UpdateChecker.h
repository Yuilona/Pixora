#pragma once

#include <QObject>
#include <QString>
#include <QVersionNumber>

#include <memory>

class QNetworkAccessManager;

namespace pixora {

class SettingsService;

// 版本比较:tag 形如 "v0.4.0" 或 "0.4.0";任一侧解析失败视为无更新
// (宁可漏报不误报)。纯函数,可单测。
inline bool isNewerVersion(const QString& tag, const QString& current) {
    QString t = tag.trimmed();
    if (t.startsWith(QLatin1Char('v')) || t.startsWith(QLatin1Char('V'))) {
        t.remove(0, 1);
    }
    const QVersionNumber latest = QVersionNumber::fromString(t);
    const QVersionNumber mine = QVersionNumber::fromString(current);
    return !latest.isNull() && !mine.isNull() && latest > mine;
}

// 启动时检查 GitHub Releases 最新版本(设置项可关,默认开)。
// 仅发现新版时发 updateAvailable;网络失败/解析失败静默,只写日志
// ——更新检查不该打扰正常使用。
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(const SettingsService* settings,
                           QObject* parent = nullptr);
    ~UpdateChecker() override;

    void checkOnStartup();

signals:
    void updateAvailable(const QString& version, const QString& url);

private:
    const SettingsService* settings_;
    std::unique_ptr<QNetworkAccessManager> nam_;
};

} // namespace pixora
