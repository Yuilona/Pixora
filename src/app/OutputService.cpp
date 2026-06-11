#include "app/OutputService.h"

#include "app/FileNameTemplate.h"
#include "app/SettingsService.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

namespace pixora {

namespace {

// jpg/webp 用设置的质量;png 的 quality 语义是压缩级别,交给默认值
int qualityFor(const QString& path, const SettingsService* settings) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") ||
        ext == QLatin1String("webp")) {
        return settings ? settings->outputQuality() : 90;
    }
    return -1;
}

} // namespace

OutputService::OutputService(const SettingsService* settings) : settings_(settings) {}

void OutputService::copyToClipboard(const QImage& image) {
    QGuiApplication::clipboard()->setImage(image);
    spdlog::info("copied {}x{} image to clipboard", image.width(), image.height());
}

QString OutputService::suggestedFileName() const {
    QString dir = settings_ ? settings_->outputDir() : QString();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    const QString stem = expandFileNameTemplate(
        settings_ ? settings_->fileNameTemplate() : QString(),
        QDateTime::currentDateTime());
    const QString ext = settings_ ? settings_->outputFormat() : QStringLiteral("png");
    return QDir(dir).filePath(stem + QLatin1Char('.') + ext);
}

QString OutputService::saveWithDialog(const QImage& image) {
    // 设置的格式排在过滤器首位 → 成为对话框默认格式
    const QString preferred = settings_ ? settings_->outputFormat() : QStringLiteral("png");
    QStringList filters = {QStringLiteral("PNG 图片 (*.png)"),
                           QStringLiteral("JPEG 图片 (*.jpg)"),
                           QStringLiteral("WebP 图片 (*.webp)")};
    for (int i = 1; i < filters.size(); ++i) {
        if (filters[i].contains(QStringLiteral("*.%1").arg(preferred))) {
            filters.move(i, 0);
            break;
        }
    }
    const QString path = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("保存截图"), suggestedFileName(),
        filters.join(QStringLiteral(";;")));
    if (path.isEmpty()) {
        return {};
    }
    if (!image.save(path, nullptr, qualityFor(path, settings_))) {
        spdlog::error("failed to save image to {}", path.toStdString());
        return {};
    }
    spdlog::info("saved image to {}", path.toStdString());
    return path;
}

QString OutputService::autoSave(const QImage& image) {
    if (!settings_ || !settings_->autoSave()) {
        return {};
    }
    QString path = suggestedFileName();
    const QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    for (int n = 1; QFileInfo::exists(path) && n < 100; ++n) {
        path = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() +
               QStringLiteral("_%1.").arg(n) + info.suffix();
    }
    if (!image.save(path, nullptr, qualityFor(path, settings_))) {
        spdlog::error("auto-save failed: {}", path.toStdString());
        return {};
    }
    spdlog::info("auto-saved image to {}", path.toStdString());
    return path;
}

} // namespace pixora
