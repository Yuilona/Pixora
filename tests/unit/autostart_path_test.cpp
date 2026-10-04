#include "platform/windows/AutoStartPath.h"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

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

TEST_CASE("autostart command with launch argument still matches its exe", "[autostart]") {
    using pixora::autostart::executableFromCommand;
    const QString exe = QStringLiteral("C:/Program Files/Pixora/pixora.exe");
    // 新写入形态带 --autostart;旧版本/安装器写的不带参数,两种都要认
    CHECK(sameExecutablePath(
        QStringLiteral("\"C:\\Program Files\\Pixora\\pixora.exe\" --autostart"), exe));
    CHECK(executableFromCommand(
              QStringLiteral("\"C:\\Program Files\\Pixora\\pixora.exe\" --autostart")) ==
          QStringLiteral("C:\\Program Files\\Pixora\\pixora.exe"));
    CHECK(executableFromCommand(QStringLiteral("  \"C:\\a b\\x.exe\"  ")) ==
          QStringLiteral("C:\\a b\\x.exe"));
    // 缺右引号:取引号后全部,不崩
    CHECK(executableFromCommand(QStringLiteral("\"C:\\a\\x.exe")) ==
          QStringLiteral("C:\\a\\x.exe"));
    CHECK(executableFromCommand(QString()).isEmpty());
}

TEST_CASE("StartupApproved flag: odd first byte means disabled", "[autostart]") {
    using pixora::autostart::startupApprovedDisabled;
    const auto bytes = [](std::initializer_list<unsigned char> b) {
        QByteArray a;
        for (unsigned char c : b) {
            a.append(static_cast<char>(c));
        }
        return a;
    };
    CHECK_FALSE(startupApprovedDisabled(QByteArray())); // 无值 = 启用
    CHECK_FALSE(startupApprovedDisabled(bytes({0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK_FALSE(startupApprovedDisabled(bytes({0x06, 0, 0, 0})));
    CHECK(startupApprovedDisabled(bytes({0x03, 0, 0, 0, 0x8E, 0x1F, 0x2A, 0x01, 0, 0, 0, 0})));
    CHECK(startupApprovedDisabled(bytes({0x07})));
}

TEST_CASE("autostart state classification", "[autostart]") {
    using pixora::AutoStartState;
    using pixora::autostart::classify;
    const QString exe = QStringLiteral("C:/Apps/Pixora/pixora.exe");
    const QString mine = QStringLiteral("\"C:\\Apps\\Pixora\\pixora.exe\" --autostart");
    const QString other = QStringLiteral("\"E:\\Dev\\Pixora\\build\\dev\\pixora.exe\"");
    const QByteArray disabled(1, '\x03');
    const QByteArray enabled(1, '\x02');

    CHECK(classify(QString(), {}, exe, false) == AutoStartState::Off);
    CHECK(classify(QStringLiteral("\"\""), {}, exe, false) == AutoStartState::Off);
    CHECK(classify(mine, {}, exe, true) == AutoStartState::Enabled);
    CHECK(classify(mine, enabled, exe, true) == AutoStartState::Enabled);
    // Run 项还在,但任务管理器里关掉了 → 不能显示为已开启
    CHECK(classify(mine, disabled, exe, true) == AutoStartState::DisabledBySystem);
    // 指向别的副本:存在 → OtherCopy(不抢);不存在 → StalePath(可修复)
    CHECK(classify(other, {}, exe, true) == AutoStartState::OtherCopy);
    CHECK(classify(other, {}, exe, false) == AutoStartState::StalePath);
    // 别的副本被系统禁用,仍按"别的副本"归类
    CHECK(classify(other, disabled, exe, true) == AutoStartState::OtherCopy);
}
