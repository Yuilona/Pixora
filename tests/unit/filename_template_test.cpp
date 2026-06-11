#include "app/FileNameTemplate.h"

#include <QDateTime>

#include <catch2/catch_test_macros.hpp>

using pixora::expandFileNameTemplate;

namespace {
const QDateTime kNow(QDate(2026, 6, 11), QTime(9, 5, 7));
}

TEST_CASE("template placeholders expand to time components", "[output]") {
    CHECK(expandFileNameTemplate(QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}"),
                                 kNow) == QStringLiteral("Pixora_20260611_090507"));
    CHECK(expandFileNameTemplate(QStringLiteral("shot-{HH}-{mm}"), kNow) ==
          QStringLiteral("shot-09-05"));
}

TEST_CASE("empty template falls back to default", "[output]") {
    CHECK(expandFileNameTemplate(QString(), kNow) ==
          QStringLiteral("Pixora_20260611_090507"));
    CHECK(expandFileNameTemplate(QStringLiteral("   "), kNow) ==
          QStringLiteral("Pixora_20260611_090507"));
}

TEST_CASE("plain text and unknown braces survive; illegal chars sanitized",
          "[output]") {
    CHECK(expandFileNameTemplate(QStringLiteral("abc_{yyyy}_{foo}"), kNow) ==
          QStringLiteral("abc_2026_{foo}"));
    CHECK(expandFileNameTemplate(QStringLiteral("a/b:c*d"), kNow) ==
          QStringLiteral("a-b-c-d"));
}
