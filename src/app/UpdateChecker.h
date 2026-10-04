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

// 检查 GitHub Releases 最新版本。
// - 启动检查(设置项可关,默认开):仅发现新版时发 updateAvailable;
//   网络失败/解析失败静默,只写日志——更新检查不该打扰正常使用;
// - 手动检查(设置面板"关于"页):无视开关,结果一律经 manualCheckFinished 回报。
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(const SettingsService* settings,
                           QObject* parent = nullptr);
    ~UpdateChecker() override;

    void checkOnStartup();
    void checkNow();

    enum class Outcome { UpToDate, NewVersion, Failed };
    Q_ENUM(Outcome)

signals:
    void updateAvailable(const QString& version, const QString& url);
    // version/url 仅 NewVersion 时有效;Failed 时 version 为失败原因
    void manualCheckFinished(pixora::UpdateChecker::Outcome outcome, const QString& version,
                             const QString& url);

private:
    void request(bool manual);

    const SettingsService* settings_;
    std::unique_ptr<QNetworkAccessManager> nam_;
};

} // namespace pixora
