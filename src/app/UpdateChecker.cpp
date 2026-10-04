#include "app/UpdateChecker.h"

#include "app/SettingsService.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <spdlog/spdlog.h>

namespace pixora {

namespace {
const QString kLatestReleaseUrl =
    QStringLiteral("https://api.github.com/repos/Yuilona/Pixora/releases/latest");
} // namespace

UpdateChecker::UpdateChecker(const SettingsService* settings, QObject* parent)
    : QObject(parent), settings_(settings),
      nam_(std::make_unique<QNetworkAccessManager>()) {
    nam_->setTransferTimeout(15'000);
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::checkOnStartup() {
    if (settings_ && !settings_->checkUpdates()) {
        spdlog::info("update check disabled in settings");
        return;
    }
    request(/*manual=*/false);
}

void UpdateChecker::checkNow() {
    request(/*manual=*/true);
}

void UpdateChecker::request(bool manual) {
    QNetworkRequest request{QUrl(kLatestReleaseUrl)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    // GitHub API 拒绝无 User-Agent 的请求(403)
    request.setRawHeader("User-Agent",
                         "Pixora/" +
                             QCoreApplication::applicationVersion().toUtf8());

    spdlog::info("update check{}: GET {}", manual ? " (manual)" : "",
                 kLatestReleaseUrl.toStdString());
    QNetworkReply* reply = nam_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // 启动检查静默:离线/限流不打扰用户
            spdlog::info("update check failed: {}",
                         reply->errorString().toStdString());
            if (manual) {
                emit manualCheckFinished(Outcome::Failed, reply->errorString(), {});
            }
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = root.value(QLatin1String("tag_name")).toString();
        const QString url = root.value(QLatin1String("html_url")).toString();
        const QString current = QCoreApplication::applicationVersion();
        if (tag.isEmpty()) {
            spdlog::info("update check: no tag_name in response");
            if (manual) {
                emit manualCheckFinished(Outcome::Failed, tr("Unexpected response"), {});
            }
            return;
        }
        if (isNewerVersion(tag, current)) {
            spdlog::info("update available: {} (current {})", tag.toStdString(),
                         current.toStdString());
            if (manual) {
                emit manualCheckFinished(Outcome::NewVersion, tag, url);
            } else {
                emit updateAvailable(tag, url);
            }
        } else {
            spdlog::info("update check: up to date ({} vs {})", tag.toStdString(),
                         current.toStdString());
            if (manual) {
                emit manualCheckFinished(Outcome::UpToDate, tag, url);
            }
        }
    });
}

} // namespace pixora
