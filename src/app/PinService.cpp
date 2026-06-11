#include "app/PinService.h"

#include "ui/pin/PinWindow.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>

#include <spdlog/spdlog.h>

namespace pixora {

PinService::PinService(QObject* parent) : QObject(parent) {}

void PinService::pinImage(const QImage& image, const QPoint& topLeftLogical) {
    if (image.isNull()) {
        return;
    }
    auto* pin = new PinWindow(image, topLeftLogical); // WA_DeleteOnClose 自管理
    pin->show();
    spdlog::info("pinned {}x{} image at ({}, {})", image.width(), image.height(),
                 topLeftLogical.x(), topLeftLogical.y());
}

bool PinService::pinFromClipboard() {
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        spdlog::info("pin from clipboard: no image in clipboard");
        return false;
    }
    pinImage(image, QCursor::pos() + QPoint(20, 20));
    return true;
}

} // namespace pixora
