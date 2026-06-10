#include <catch2/catch_test_macros.hpp>

// M0 占位:验证测试链路(Catch2 + CTest)打通。
// core 层模块落地后在 unit/ 下按模块添加真实测试。
TEST_CASE("test infrastructure is wired up", "[smoke]") {
    CHECK(1 + 1 == 2);
}
