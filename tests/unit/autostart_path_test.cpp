#include "platform/windows/AutoStartPath.h"

#include <catch2/catch_test_macros.hpp>

using pixora::autostart::sameExecutablePath;

// 注册表 Run 项里存的是引号包裹的反斜杠路径,applicationFilePath() 给的是
// 正斜杠无引号路径。两侧必须同样归一化后比对,否则正常自启项会被误判未启用。
TEST_CASE("autostart path match is quote/separator/case insensitive", "[autostart]") {
    const QString exe = QStringLiteral("C:/Program Files/Pixora/pixora.exe");

    // 写入形态(引号 + 反斜杠)== 当前 exe(正斜杠)
    CHECK(sameExecutablePath(
        QStringLiteral("\"C:\\Program Files\\Pixora\\pixora.exe\""), exe));
    // 无引号也认
    CHECK(sameExecutablePath(
        QStringLiteral("C:\\Program Files\\Pixora\\pixora.exe"), exe));
    // 大小写不敏感(Windows 路径)
    CHECK(sameExecutablePath(
        QStringLiteral("\"c:\\program files\\pixora\\PIXORA.exe\""), exe));
}

TEST_CASE("autostart path match rejects stale/empty/different paths", "[autostart]") {
    const QString exe = QStringLiteral("C:/Apps/Pixora/pixora.exe");
    // 旧路径残留(便携版被移动)→ 视为未启用,复选框才会如实显示未勾选
    CHECK_FALSE(sameExecutablePath(
        QStringLiteral("\"C:\\Old\\Pixora\\pixora.exe\""), exe));
    CHECK_FALSE(sameExecutablePath(QString(), exe));       // 无值
    CHECK_FALSE(sameExecutablePath(QStringLiteral("\"\""), exe)); // 空引号
}

TEST_CASE("autostart path match handles spaces and non-ASCII", "[autostart]") {
    // 含空格 + 中文目录(国内用户常见解压位置)应正确往返匹配
    const QString exe = QStringLiteral("C:/Users/张三/我的 程序/Pixora/pixora.exe");
    CHECK(sameExecutablePath(
        QStringLiteral("\"C:\\Users\\张三\\我的 程序\\Pixora\\pixora.exe\""), exe));
    CHECK_FALSE(sameExecutablePath(
        QStringLiteral("\"C:\\Users\\李四\\Pixora\\pixora.exe\""), exe));
}
