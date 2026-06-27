#pragma once

#include <QDir>
#include <QLatin1Char>
#include <QString>

namespace pixora::autostart {

// 判断注册表 Run 项里存的自启目标是否就是当前可执行文件。
//
// 写入时路径用反斜杠且带引号("C:\\Dir\\app.exe"),而
// QCoreApplication::applicationFilePath() 返回正斜杠且不带引号——必须把两侧
// 做同样的归一化(去引号、反斜杠→正斜杠、cleanPath、大小写不敏感)后再比,
// 否则正常的自启项会被误判为"未启用",把开发机独有的误报变成所有人都误报的漏报。
//
// 反斜杠在任意宿主上都按分隔符处理(不依赖 QDir::fromNativeSeparators 的宿主语义),
// 以便在非 Windows 上也能对这段纯逻辑做单元测试。
inline bool sameExecutablePath(QString storedValue, QString currentExe) {
    const auto normalize = [](QString p) {
        p = p.trimmed();
        if (p.size() >= 2 && p.startsWith(QLatin1Char('"')) &&
            p.endsWith(QLatin1Char('"'))) {
            p = p.mid(1, p.size() - 2);
        }
        p.replace(QLatin1Char('\\'), QLatin1Char('/'));
        return QDir::cleanPath(p);
    };
    const QString a = normalize(storedValue);
    const QString b = normalize(currentExe);
    return !a.isEmpty() && QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

} // namespace pixora::autostart
