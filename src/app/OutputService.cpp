#include "app/OutputService.h"

#include "app/SettingsService.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

namespace pixora {

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
    const QString name = QStringLiteral("Pixora_%1.png")
                             .arg(QDateTime::currentDateTime().toString(
                                 QStringLiteral("yyyyMMdd_HHmmss")));
    return QDir(dir).filePath(name);
}

QString OutputService::saveWithDialog(const QImage& image) {
    const QString path = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("保存截图"), suggestedFileName(),
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
    if (path.isEmpty()) {
        return {};
    }
    if (!image.save(path)) {
        spdlog::error("failed to save image to {}", path.toStdString());
        return {};
    }
    spdlog::info("saved image to {}", path.toStdString());
    return path;
}

} // namespace pixora
