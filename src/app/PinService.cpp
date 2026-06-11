#include "app/PinService.h"

#include "ui/pin/PinWindow.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>

#include <spdlog/spdlog.h>

namespace pixora {

PinService::PinService(ISystemIntegration* system, QObject* parent)
    : QObject(parent), system_(system) {}

void PinService::pinImage(const QImage& image, const QPoint& topLeftLogical) {
    if (image.isNull()) {
        return;
    }
    auto* pin = new PinWindow(image, topLeftLogical, system_); // WA_DeleteOnClose 自管理
    pins_.emplace_back(pin);
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

void PinService::closeAll() {
    int closed = 0;
    for (const QPointer<PinWindow>& pin : pins_) {
        if (pin) {
            pin->close();
            ++closed;
        }
    }
    pins_.clear();
    spdlog::info("closed {} pin window(s)", closed);
}

} // namespace pixora
