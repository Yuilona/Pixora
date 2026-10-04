#pragma once

#include "platform/interface/SystemIntegration.h"

#include <QByteArray>
#include <QDir>
#include <QLatin1Char>
#include <QString>

namespace pixora::autostart {

// 从 Run 项的命令行里取出可执行文件路径。
// 写入形态为 "C:\Dir\app.exe" --autostart(引号包裹路径 + 可选参数);
// 旧版本/安装器写的是不带参数的 "C:\Dir\app.exe"。无引号时整串视为路径
// (无引号的路径可能含空格,无法可靠切分参数,按整串处理最保守)。
inline QString executableFromCommand(QString command) {
    command = command.trimmed();
    if (command.startsWith(QLatin1Char('"'))) {
        const qsizetype close = command.indexOf(QLatin1Char('"'), 1);
        return close < 0 ? command.mid(1) : command.mid(1, close - 1);
    }
    return command;
}

// 判断注册表 Run 项里存的自启目标是否就是当前可执行文件。
//
// 写入时路径用反斜杠且带引号("C:\\Dir\\app.exe"),而
// QCoreApplication::applicationFilePath() 返回正斜杠且不带引号——必须把两侧
// 做同样的归一化(去引号、反斜杠→正斜杠、cleanPath、大小写不敏感)后再比,
// 否则正常的自启项会被误判为"未启用",把开发机独有的误报变成所有人都误报的漏报。
//
// 反斜杠在任意宿主上都按分隔符处理(不依赖 QDir::fromNativeSeparators 的宿主语义),
// 以便在非 Windows 上也能对这段纯逻辑做单元测试。
inline bool sameExecutablePath(const QString& storedValue, const QString& currentExe) {
    const auto normalize = [](QString p) {
        p = executableFromCommand(p);
        p.replace(QLatin1Char('\\'), QLatin1Char('/'));
        return QDir::cleanPath(p);
    };
    const QString a = normalize(storedValue);
    const QString b = normalize(currentExe);
    return !a.isEmpty() && QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

// 任务管理器"启动应用"/设置 > 应用 > 启动 关闭某项时不删 Run 值,而是在
// Explorer\StartupApproved\Run 下写同名 REG_BINARY:首字节最低位为 1
//(0x03/0x07)= 已禁用,为 0(0x02/0x06)= 启用;值不存在 = 启用。
inline bool startupApprovedDisabled(const QByteArray& data) {
    return !data.isEmpty() && (static_cast<unsigned char>(data.at(0)) & 0x01) != 0;
}

// 由 Run 值、StartupApproved 值与当前 exe 归类自启状态(纯逻辑,便于单测)。
// storedExeExists:Run 项指向的文件当前是否存在(由调用方查文件系统)。
inline AutoStartState classify(const QString& runValue, const QByteArray& approvedData,
                               const QString& currentExe, bool storedExeExists) {
    if (executableFromCommand(runValue).isEmpty()) {
        return AutoStartState::Off;
    }
    if (sameExecutablePath(runValue, currentExe)) {
        return startupApprovedDisabled(approvedData) ? AutoStartState::DisabledBySystem
                                                     : AutoStartState::Enabled;
    }
    return storedExeExists ? AutoStartState::OtherCopy : AutoStartState::StalePath;
}

} // namespace pixora::autostart
