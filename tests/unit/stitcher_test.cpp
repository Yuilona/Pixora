#include "core/stitch/Stitcher.h"

#include <QImage>
#include <QPainter>

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

namespace {

// 组装带 sticky 头/尾的帧:header + 内容窗口 + footer
QImage frameWithChrome(const QImage& content, int contentTop, int vp,
                       const QImage& header, const QImage& footer) {
    const int headerH = header.isNull() ? 0 : header.height();
    const int footerH = footer.isNull() ? 0 : footer.height();
    QImage frame(content.width(), vp, QImage::Format_ARGB32);
    QPainter p(&frame);
    if (headerH > 0) {
        p.drawImage(0, 0, header);
    }
    p.drawImage(0, headerH, content, 0, contentTop, content.width(),
                vp - headerH - footerH);
    if (footerH > 0) {
        p.drawImage(0, vp - footerH, footer);
    }
    return frame;
}

} // namespace

TEST_CASE("sticky footer is deduplicated and kept once at bottom", "[stitch]") {
    const QImage content = makeContent(320, 1500);
    const QImage footer = makeContent(320, 40, 99);
    constexpr int vp = 300;
    constexpr int contentVp = vp - 40; // 260

    Stitcher stitcher;
    stitcher.begin(frameWithChrome(content, 0, vp, {}, footer));
    int top = 0;
    for (int i = 0; i < 6; ++i) {
        top += 120;
        REQUIRE(stitcher.append(frameWithChrome(content, top, vp, {}, footer)) ==
                Stitcher::AppendResult::Appended);
    }

    const QImage result = stitcher.result();
    // 成图 = 连续内容 + 底栏一次
    REQUIRE(result.height() == top + contentVp + 40);
    CHECK(result.copy(0, 0, 320, top + contentVp) ==
          content.copy(0, 0, 320, top + contentVp)
              .convertToFormat(QImage::Format_ARGB32));
    CHECK(result.copy(0, top + contentVp, 320, 40) ==
          footer.convertToFormat(QImage::Format_ARGB32));
}

TEST_CASE("sticky header stays once and does not break matching", "[stitch]") {
    const QImage content = makeContent(320, 1500);
    const QImage header = makeContent(320, 40, 77);
    constexpr int vp = 300;
    constexpr int contentVp = vp - 40;

    Stitcher stitcher;
    stitcher.begin(frameWithChrome(content, 0, vp, header, {}));
    int top = 0;
    for (int i = 0; i < 6; ++i) {
        top += 120;
        REQUIRE(stitcher.append(frameWithChrome(content, top, vp, header, {})) ==
                Stitcher::AppendResult::Appended);
    }

    const QImage result = stitcher.result();
    REQUIRE(result.height() == 40 + top + contentVp);
    CHECK(result.copy(0, 0, 320, 40) == header.convertToFormat(QImage::Format_ARGB32));
    CHECK(result.copy(0, 40, 320, top + contentVp) ==
          content.copy(0, 0, 320, top + contentVp)
              .convertToFormat(QImage::Format_ARGB32));
}

TEST_CASE("local animation in one band is outvoted by median", "[stitch]") {
    const QImage content = makeContent(330, 1200);
    constexpr int vp = 300;
    const int noiseX = 330 - 24 - 100; // 噪声区落在右带内(右保护带之外)

    auto frameAt = [&](int top, quint32 animSeed) {
        QImage frame = content.copy(0, top, 330, vp);
        QPainter p(&frame);
        p.drawImage(noiseX, 60, makeContent(100, 180, animSeed)); // 模拟 gif 动画
        return frame;
    };

    Stitcher stitcher;
    stitcher.begin(frameAt(0, 1000));
    int top = 0;
    for (int i = 1; i <= 5; ++i) {
        top += 130;
        REQUIRE(stitcher.append(frameAt(top, 1000 + static_cast<quint32>(i))) ==
                Stitcher::AppendResult::Appended);
    }
    // 左带未受污染,逐像素验证拼接对齐正确
    const QImage result = stitcher.result();
    REQUIRE(result.height() == top + vp);
    CHECK(result.copy(0, 0, noiseX, top + vp) ==
          content.copy(0, 0, noiseX, top + vp).convertToFormat(QImage::Format_ARGB32));
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
