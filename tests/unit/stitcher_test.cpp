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

TEST_CASE("scrolling back up yields NoNewContent", "[stitch]") {
    const QImage content = makeContent(320, 900);
    Stitcher stitcher;
    stitcher.begin(viewport(content, 200, 300));
    // 小幅回滚:模板带仍可见,定位在更低处 → 无新内容
    CHECK(stitcher.append(viewport(content, 195, 300)) ==
          Stitcher::AppendResult::NoNewContent);
    // 大幅回滚:底部模板带虽滚出视野,但多带回退在更高处找到同一内容,
    // 偏移为负 → 正确判定无新内容(优于旧版的"匹配失败")
    CHECK(stitcher.append(viewport(content, 50, 300)) ==
          Stitcher::AppendResult::NoNewContent);
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

namespace {

// 在 content 的 [y0, y0+h) 行段画满周期性横向条纹(行色仅由 y%period 决定)。
// 模拟 Pixora logo 那种"一格格彩色方块"的网格:NCC 在 dy=k*period 处都得满分,
// minMaxLoc 取错峰值 → dy 判错 → 内容被重复追加(成图虚高)。
void paintPeriodicStripes(QImage& content, int y0, int h, int period) {
    for (int dy = 0; dy < h; ++dy) {
        const int y = y0 + dy;
        if (y < 0 || y >= content.height()) {
            continue;
        }
        const int p = dy % period;
        // 周期内做个明显的明暗渐变,保证带内有纹理(方差不为零,能过阈值)
        const int v = (p * 255) / std::max(1, period - 1);
        auto* line = reinterpret_cast<QRgb*>(content.scanLine(y));
        for (int x = 0; x < content.width(); ++x) {
            line[x] = qRgb(v, 255 - v, (v * 2) % 256);
        }
    }
}

} // namespace

TEST_CASE("periodic block (logo grid) does not duplicate content", "[stitch][repro]") {
    // 纹理页中嵌入一段全宽周期性条纹(模拟带 logo 网格的 README 横幅)。
    // 块高 120 < 帧间重叠(vp-步长=200):重叠区里总有正文纹理可作锚点,
    // 周期块应被一次性正确带入——这正是真实场景(区域远高于 logo)的情形。
    QImage content = makeContent(320, 1600);
    paintPeriodicStripes(content, 600, 120, 20);

    constexpr int vp = 300;
    Stitcher stitcher;
    stitcher.begin(viewport(content, 0, vp));
    int top = 0;
    for (int i = 0; i < 13; ++i) {
        top += 100; // 1300 为止,均匀下滚(步长 100 < vp,重叠 200)
        stitcher.append(viewport(content, top, vp));
    }

    // 周期块由上下纹理锚点的正确 dy 一次性带入:高度精确等于真实滚动高度。
    // 修复前 NCC 在周期块上取错峰 → 虚高到 1740(内容被重复追加)。
    CHECK(stitcher.resultHeight() == top + vp);
}

TEST_CASE("centered logo on white margins does not duplicate", "[stitch][repro]") {
    // 贴近真实 README 横幅:正文纹理页中,有一段白底横幅,横幅中央是
    // 周期性 logo(仅中间带,两侧白边)。重现"侧带白色失配 + 中带周期歧义"
    // 的真实几何,且用大视口(重叠充裕,如真实 785px 区域)。
    QImage content = makeContent(400, 2000);
    // 白底横幅 rows[700,1000)
    {
        QPainter p(&content);
        p.fillRect(0, 700, 400, 300, Qt::white);
    }
    // 居中周期 logo:x[150,250), rows[800,920)
    QImage logo(400, 2000, QImage::Format_ARGB32);
    paintPeriodicStripes(logo, 800, 120, 20);
    {
        QPainter p(&content);
        p.drawImage(QRect(150, 800, 100, 120), logo, QRect(150, 800, 100, 120));
    }

    constexpr int vp = 600;
    Stitcher stitcher;
    stitcher.begin(viewport(content, 0, vp));
    int top = 0;
    for (int i = 0; i < 9; ++i) {
        top += 150; // 到 1350,重叠 vp-150=450 远大于横幅,不会卡
        stitcher.append(viewport(content, top, vp));
    }

    // 不得重复(超高),也不应大段丢失;白边/居中 logo 由正文锚点正确带入
    CHECK(stitcher.resultHeight() <= top + vp);
    CHECK(stitcher.resultHeight() >= top + vp - 30);
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
