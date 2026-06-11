#include "core/stitch/Stitcher.h"

#include <QImage>

#include <catch2/catch_test_macros.hpp>

using pixora::Stitcher;

namespace {

// 确定性伪随机纹理。必须用雪崩充分的整数哈希:简单的 x*A^y*B
// 准周期重复,会让 NCC 在错误偏移上也得高分。
QImage makeContent(int w, int h, quint32 seed = 0) {
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            quint32 v = static_cast<quint32>(x) * 0x9E3779B1u +
                        static_cast<quint32>(y) * 0x85EBCA77u + seed * 0xC2B2AE3Du;
            v ^= v >> 15;
            v *= 0x2C1B3C6Du;
            v ^= v >> 12;
            v *= 0x297A2D39u;
            v ^= v >> 15;
            line[x] = qRgb(v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF);
        }
    }
    return img;
}

QImage viewport(const QImage& content, int top, int height) {
    return content.copy(0, top, content.width(), height);
}

} // namespace

TEST_CASE("stitcher reconstructs content from known scroll offsets", "[stitch]") {
    const QImage content = makeContent(320, 1200);
    constexpr int vp = 300;

    Stitcher stitcher;
    stitcher.begin(viewport(content, 0, vp));
    // 步长不一(模拟真实滚动),均小于 帧高-模板带-保护带
    const int offsets[] = {130, 250, 420, 600, 720, 900};
    for (const int top : offsets) {
        REQUIRE(stitcher.append(viewport(content, top, vp)) ==
                Stitcher::AppendResult::Appended);
    }

    REQUIRE(stitcher.resultHeight() == 900 + vp);
    const QImage result = stitcher.result();
    // 逐像素等于原始内容(拼接是裸行拷贝,应零损)
    CHECK(result == content.convertToFormat(QImage::Format_ARGB32));
}

TEST_CASE("identical frame yields NoNewContent", "[stitch]") {
    const QImage content = makeContent(320, 600);
    Stitcher stitcher;
    stitcher.begin(viewport(content, 0, 300));
    CHECK(stitcher.append(viewport(content, 0, 300)) ==
          Stitcher::AppendResult::NoNewContent);
    CHECK(stitcher.resultHeight() == 300);
}

TEST_CASE("unrelated frame yields MatchFailed", "[stitch]") {
    Stitcher stitcher;
    stitcher.begin(viewport(makeContent(320, 600, 1), 0, 300));
    CHECK(stitcher.append(viewport(makeContent(320, 600, 2), 100, 300)) ==
          Stitcher::AppendResult::MatchFailed);
}

TEST_CASE("frame size mismatch is rejected", "[stitch]") {
    Stitcher stitcher;
    stitcher.begin(makeContent(320, 300));
    CHECK(stitcher.append(makeContent(200, 300)) ==
          Stitcher::AppendResult::MatchFailed);
}

TEST_CASE("scrolling back up: slight = NoNewContent, far = MatchFailed", "[stitch]") {
    const QImage content = makeContent(320, 900);
    Stitcher stitcher;
    stitcher.begin(viewport(content, 200, 300));
    // 小幅回滚:模板带仍可见,定位在更低处 → 无新内容
    CHECK(stitcher.append(viewport(content, 195, 300)) ==
          Stitcher::AppendResult::NoNewContent);
    // 大幅回滚:模板带(上一帧底部)已滚出视野 → 匹配失败
    CHECK(stitcher.append(viewport(content, 50, 300)) ==
          Stitcher::AppendResult::MatchFailed);
}

TEST_CASE("max canvas height caps growth", "[stitch]") {
    const QImage content = makeContent(160, 1000);
    Stitcher::Config config;
    config.maxCanvasHeight = 500;
    Stitcher stitcher(config);
    stitcher.begin(viewport(content, 0, 300));
    CHECK(stitcher.append(viewport(content, 150, 300)) ==
          Stitcher::AppendResult::Appended); // 450 ≤ 500
    CHECK(stitcher.append(viewport(content, 350, 300)) ==
          Stitcher::AppendResult::NoNewContent); // 超限被拒
    CHECK(stitcher.resultHeight() == 450);
}
