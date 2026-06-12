#include "app/UpdateChecker.h"

#include <catch2/catch_test_macros.hpp>

using pixora::isNewerVersion;

TEST_CASE("newer tag with v prefix detected", "[update]") {
    CHECK(isNewerVersion(QStringLiteral("v0.5.0"), QStringLiteral("0.4.0")));
    CHECK(isNewerVersion(QStringLiteral("v1.0.0"), QStringLiteral("0.9.9")));
}

TEST_CASE("same or older version is not an update", "[update]") {
    CHECK_FALSE(isNewerVersion(QStringLiteral("v0.4.0"), QStringLiteral("0.4.0")));
    CHECK_FALSE(isNewerVersion(QStringLiteral("v0.3.9"), QStringLiteral("0.4.0")));
}

TEST_CASE("more version segments compare correctly", "[update]") {
    CHECK(isNewerVersion(QStringLiteral("0.4.0.1"), QStringLiteral("0.4.0")));
    CHECK_FALSE(isNewerVersion(QStringLiteral("0.4"), QStringLiteral("0.4.0")));
}

TEST_CASE("garbage tag never reports an update", "[update]") {
    CHECK_FALSE(isNewerVersion(QStringLiteral("latest"), QStringLiteral("0.4.0")));
    CHECK_FALSE(isNewerVersion(QString(), QStringLiteral("0.4.0")));
    CHECK_FALSE(isNewerVersion(QStringLiteral("v0.5.0"), QString()));
}
