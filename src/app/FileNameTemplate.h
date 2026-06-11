#pragma once

#include <QDateTime>
#include <QString>

namespace pixora {

// 文件名模板展开:{yyyy}{MM}{dd}{HH}{mm}{ss} 替换为对应时间分量,
// 其余字符原样保留;空模板回退默认;文件名非法字符替换为 '-'。
// 纯函数,便于单测(对话框/自动存盘共用)。
inline QString expandFileNameTemplate(const QString& tmpl, const QDateTime& now) {
    QString out = tmpl.trimmed();
    if (out.isEmpty()) {
        out = QStringLiteral("Pixora_{yyyy}{MM}{dd}_{HH}{mm}{ss}");
    }
    const struct {
        const char* key;
        const char* fmt;
    } kMap[] = {
        {"{yyyy}", "yyyy"}, {"{MM}", "MM"}, {"{dd}", "dd"},
        {"{HH}", "HH"},     {"{mm}", "mm"}, {"{ss}", "ss"},
    };
    for (const auto& m : kMap) {
        out.replace(QLatin1String(m.key), now.toString(QLatin1String(m.fmt)));
    }
    for (QChar forbidden : QStringLiteral("\\/:*?\"<>|")) {
        out.replace(forbidden, QLatin1Char('-'));
    }
    return out;
}

} // namespace pixora
