#include "core/text/ReplaceRenderer.h"

#include <QGuiApplication>

#include <catch2/catch_test_macros.hpp>

using pixora::OcrLine;
using pixora::renderReplacedText;

namespace {

// 文字回绘需要字体引擎 → QGuiApplication;offscreen 平台保证无显示环境可跑
void ensureGuiApp() {
    if (!QGuiApplication::instance()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        static int argc = 1;
        static char arg0[] = "pixora_tests";
        static char* argv[] = {arg0, nullptr};
        static QGuiApplication app(argc, argv);
    }
}

QImage makeBase() {
    // 白底,(10,10)-(160,34) 一条黑色"文字"区域
    QImage image(200, 80, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    for (int y = 12; y <= 30; ++y) {
        for (int x = 12; x <= 156; ++x) {
            image.setPixelColor(x, y, Qt::black);
        }
    }
    return image;
}

} // namespace

TEST_CASE("replaced line covered with sampled background", "[replace]") {
    ensureGuiApp();
    const QList<OcrLine> lines = {{QStringLiteral("dummy"), QRect(10, 10, 150, 24)}};
    const QImage out =
        renderReplacedText(makeBase(), lines, {QStringLiteral("好")});

    REQUIRE(out.size() == QSize(200, 80));
    // 原黑色区域右端已被白色背景抹平(译文"好"很短,画在左侧)
    CHECK(out.pixelColor(150, 21).lightness() > 200);
    // 框外像素不受影响
    CHECK(out.pixelColor(180, 60) == QColor(Qt::white));
}

TEST_CASE("empty translation keeps original pixels", "[replace]") {
    ensureGuiApp();
    const QList<OcrLine> lines = {{QStringLiteral("dummy"), QRect(10, 10, 150, 24)}};
    const QImage out = renderReplacedText(makeBase(), lines, {QString()});
    CHECK(out.pixelColor(80, 21) == QColor(Qt::black)); // 原样保留
}

TEST_CASE("invalid box line skipped without crash", "[replace]") {
    ensureGuiApp();
    const QList<OcrLine> lines = {{QStringLiteral("dummy"), QRect()}};
    const QImage out =
        renderReplacedText(makeBase(), lines, {QStringLiteral("译文")});
    CHECK(out.pixelColor(80, 21) == QColor(Qt::black));
}

TEST_CASE("device pixel ratio preserved", "[replace]") {
    ensureGuiApp();
    QImage base = makeBase();
    base.setDevicePixelRatio(2.0);
    const QList<OcrLine> lines = {{QStringLiteral("dummy"), QRect(10, 10, 150, 24)}};
    const QImage out =
        renderReplacedText(base, lines, {QStringLiteral("好")});
    CHECK(out.devicePixelRatio() == 2.0);
}
