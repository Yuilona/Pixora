#include "platform/shared/QtScreenCapturer.h"

#include <QPixmap>
#include <QScreen>

namespace pixora {

QImage QtScreenCapturer::grabScreen(QScreen* screen) {
    QImage image = screen->grabWindow(0).toImage();
    image.setDevicePixelRatio(1.0);
    return image;
}

QImage QtScreenCapturer::grabScreenRegion(QScreen* screen, const QRect& regionLocal) {
    QImage image = screen
                       ->grabWindow(0, regionLocal.x(), regionLocal.y(),
                                    regionLocal.width(), regionLocal.height())
                       .toImage();
    image.setDevicePixelRatio(1.0);
    return image;
}

} // namespace pixora
