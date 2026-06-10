#include "core/capture/DesktopSnapshot.h"

#include <QColor>
#include <QImage>

#include <catch2/catch_test_macros.hpp>

using pixora::DesktopSnapshot;
using pixora::ScreenSnap;

namespace {

QImage solidImage(int wPhysical, int hPhysical, QColor color) {
    QImage img(wPhysical, hPhysical, QImage::Format_ARGB32_Premultiplied);
    img.fill(color);
    return img;
}

} // namespace

TEST_CASE("copy region from single 1x screen is pixel exact", "[snapshot]") {
    // 100x100 逻辑 = 物理,左上 10x10 涂红,其余蓝
    QImage img = solidImage(100, 100, Qt::blue);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            img.setPixelColor(x, y, Qt::red);

    DesktopSnapshot snap({ScreenSnap{img, QRect(0, 0, 100, 100), 1.0}});
    const QImage out = snap.copyRegionLogical(QRect(0, 0, 10, 10));

    REQUIRE(out.size() == QSize(10, 10));
    CHECK(out.pixelColor(5, 5) == QColor(Qt::red));
}

TEST_CASE("copy region across mixed-DPI screens stitches both", "[snapshot]") {
    // 屏1: 逻辑 (0,0,100,100), dpr 1, 红;屏2: 逻辑 (100,0,100,100), dpr 2, 绿(物理 200x200)
    DesktopSnapshot snap({
        ScreenSnap{solidImage(100, 100, Qt::red), QRect(0, 0, 100, 100), 1.0},
        ScreenSnap{solidImage(200, 200, Qt::green), QRect(100, 0, 100, 100), 2.0},
    });

    // 跨屏取 (80,10) 起 40x20:左半红、右半绿;输出 DPR 应取 2
    const QImage out = snap.copyRegionLogical(QRect(80, 10, 40, 20));

    REQUIRE(out.devicePixelRatio() == 2.0);
    REQUIRE(out.size() == QSize(80, 40)); // 物理 = 逻辑 × 2
    CHECK(out.pixelColor(10, 20) == QColor(Qt::red));    // 物理(10,20) → 逻辑(5,10) 屏1
    CHECK(out.pixelColor(70, 20) == QColor(Qt::green));  // 物理(70,20) → 逻辑(35,10) 屏2
}

TEST_CASE("virtual geometry unites all screens", "[snapshot]") {
    DesktopSnapshot snap({
        ScreenSnap{solidImage(10, 10, Qt::black), QRect(0, 0, 1920, 1080), 1.0},
        ScreenSnap{solidImage(10, 10, Qt::black), QRect(1920, -200, 1280, 1024), 1.0},
    });
    // 并集: x∈[0,3200), y∈[-200,1080) → 3200x1280
    CHECK(snap.virtualGeometryLogical() == QRect(0, -200, 3200, 1280));
}

TEST_CASE("empty region or empty snapshot yields null image", "[snapshot]") {
    CHECK(DesktopSnapshot{}.copyRegionLogical(QRect(0, 0, 10, 10)).isNull());

    DesktopSnapshot snap({ScreenSnap{solidImage(10, 10, Qt::black), QRect(0, 0, 10, 10), 1.0}});
    CHECK(snap.copyRegionLogical(QRect()).isNull());
}
