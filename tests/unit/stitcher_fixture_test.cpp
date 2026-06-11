#include "core/stitch/Stitcher.h"

#include <QDir>
#include <QImage>

#include <catch2/catch_test_macros.hpp>

using pixora::Stitcher;

// 真实帧序列回归:重放 tests/fixtures/scroll_cases/<case>/frame_*.png,
// 与 expected.png 逐像素比对。采集方法见该目录 README。
TEST_CASE("scroll fixture cases replay to expected output", "[stitch][fixtures]") {
    const QDir root(QStringLiteral(PIXORA_FIXTURES_DIR "/scroll_cases"));
    const QStringList cases =
        root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (cases.isEmpty()) {
        SUCCEED("no fixture cases recorded yet");
        return;
    }

    for (const QString& caseName : cases) {
        DYNAMIC_SECTION("case: " << caseName.toStdString()) {
            const QDir caseDir(root.filePath(caseName));
            const QStringList frameFiles = caseDir.entryList(
                {QStringLiteral("frame_*.png")}, QDir::Files, QDir::Name);
            REQUIRE(frameFiles.size() >= 2);
            const QImage expected(caseDir.filePath(QStringLiteral("expected.png")));
            REQUIRE_FALSE(expected.isNull());

            Stitcher stitcher;
            stitcher.begin(QImage(caseDir.filePath(frameFiles.first())));
            for (int i = 1; i < frameFiles.size(); ++i) {
                stitcher.append(QImage(caseDir.filePath(frameFiles.at(i))));
            }
            const QImage result =
                stitcher.result().convertToFormat(QImage::Format_ARGB32);
            CHECK(result == expected.convertToFormat(QImage::Format_ARGB32));
        }
    }
}
