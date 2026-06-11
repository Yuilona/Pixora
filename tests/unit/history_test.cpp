#include "app/HistoryService.h"

#include "app/SettingsService.h"

#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using namespace pixora;

namespace {

QImage makeImage(int w, qreal dpr = 1.0) {
    QImage img(w, 10, QImage::Format_ARGB32);
    img.fill(Qt::red);
    img.setDevicePixelRatio(dpr);
    return img;
}

} // namespace

TEST_CASE("history records newest-first and prunes to limit", "[history]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService settings(dir.filePath("s.ini"));
    settings.setHistoryLimit(3);
    HistoryService history(&settings, dir.filePath("h"));

    for (int i = 1; i <= 5; ++i) {
        history.record(makeImage(10 + i));
    }
    REQUIRE(history.entries().size() == 3);
    // 录入宽度 11..15;新→旧保留 15, 14, 13,最旧的 11/12 已淘汰
    CHECK(history.load(history.entries()[0]).width() == 15);
    CHECK(history.load(history.entries()[2]).width() == 13);
}

TEST_CASE("history limit 0 disables recording", "[history]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService settings(dir.filePath("s.ini"));
    settings.setHistoryLimit(0);
    HistoryService history(&settings, dir.filePath("h"));

    history.record(makeImage(10));
    CHECK(history.entries().empty());
}

TEST_CASE("history persists across instances and keeps dpr", "[history]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService settings(dir.filePath("s.ini"));

    {
        HistoryService history(&settings, dir.filePath("h"));
        history.record(makeImage(20, 2.0));
    }
    HistoryService reloaded(&settings, dir.filePath("h"));
    REQUIRE(reloaded.entries().size() == 1);
    CHECK(reloaded.load(reloaded.entries()[0]).devicePixelRatio() == 2.0);
}

TEST_CASE("history remove and clear delete files", "[history]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService settings(dir.filePath("s.ini"));
    HistoryService history(&settings, dir.filePath("h"));

    history.record(makeImage(10));
    history.record(makeImage(11));
    REQUIRE(history.entries().size() == 2);

    const QString id = history.entries()[0].id;
    history.remove(id);
    REQUIRE(history.entries().size() == 1);
    CHECK(history.entries()[0].id != id);

    history.clear();
    CHECK(history.entries().empty());

    HistoryService reloaded(&settings, dir.filePath("h"));
    CHECK(reloaded.entries().empty());
}
