#include "app/SettingsService.h"

#include <QJsonObject>
#include <QKeySequence>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using pixora::SettingsService;

TEST_CASE("settings default hotkeys are F1 capture and F3 pin", "[settings]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService s(dir.filePath("test.ini"));

    CHECK(s.hotkeyCaptureRegion() == QKeySequence("F1"));
    CHECK(s.hotkeyPinFromClipboard() == QKeySequence("F3"));
}

TEST_CASE("settings persist across instances", "[settings]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString ini = dir.filePath("test.ini");

    {
        SettingsService s(ini);
        s.setHotkeyCaptureRegion(QKeySequence("Ctrl+Alt+S"));
    }
    SettingsService reloaded(ini);
    CHECK(reloaded.hotkeyCaptureRegion() == QKeySequence("Ctrl+Alt+S"));
}

TEST_CASE("settings emit changed only on real change", "[settings]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    SettingsService s(dir.filePath("test.ini"));

    int notified = 0;
    QObject::connect(&s, &SettingsService::changed, [&notified](const QString&) { ++notified; });

    s.setHotkeyCaptureRegion(QKeySequence("Ctrl+1"));
    CHECK(notified == 1);
    s.setHotkeyCaptureRegion(QKeySequence("Ctrl+1"));
    CHECK(notified == 1);
}

TEST_CASE("settings json export/import roundtrip", "[settings]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SettingsService source(dir.filePath("source.ini"));
    source.setHotkeyCaptureRegion(QKeySequence("Ctrl+Shift+A"));
    source.setHotkeyPinFromClipboard(QKeySequence("F12"));
    const QJsonObject exported = source.exportToJson();

    SettingsService target(dir.filePath("target.ini"));
    target.importFromJson(exported);
    CHECK(target.hotkeyCaptureRegion() == QKeySequence("Ctrl+Shift+A"));
    CHECK(target.hotkeyPinFromClipboard() == QKeySequence("F12"));
}
