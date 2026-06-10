#include "core/capture/DesktopSnapshot.h"

#include <QPainter>

#include <algorithm>
#include <utility>

namespace pixora {

DesktopSnapshot::DesktopSnapshot(std::vector<ScreenSnap> screens)
    : screens_(std::move(screens)) {}

QRect DesktopSnapshot::virtualGeometryLogical() const {
    QRect united;
    for (const ScreenSnap& s : screens_) {
        united |= s.geometryLogical;
    }
    return united;
}

QImage DesktopSnapshot::copyRegionLogical(const QRect& region) const {
    if (region.isEmpty() || screens_.empty()) {
        return {};
    }

    qreal outDpr = 1.0;
    for (const ScreenSnap& s : screens_) {
        if (s.geometryLogical.intersects(region)) {
            outDpr = std::max(outDpr, s.dpr);
        }
    }

    QImage out(region.size() * outDpr, QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(outDpr);
    out.fill(Qt::black);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const ScreenSnap& s : screens_) {
        const QRect overlap = s.geometryLogical & region;
        if (overlap.isEmpty()) {
            continue;
        }
        const QRectF srcPhysical((overlap.x() - s.geometryLogical.x()) * s.dpr,
                                 (overlap.y() - s.geometryLogical.y()) * s.dpr,
                                 overlap.width() * s.dpr, overlap.height() * s.dpr);
        const QRectF dstLogical(overlap.x() - region.x(), overlap.y() - region.y(),
                                overlap.width(), overlap.height());
        painter.drawImage(dstLogical, s.image, srcPhysical);
    }
    return out;
}

} // namespace pixora
