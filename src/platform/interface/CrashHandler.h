#pragma once

#include <QString>

namespace pixora {

// 进程级崩溃捕获:崩溃时把 minidump 写入 dumpDir 并留下待提示标记。
// 启动时调用一次;返回上次会话是否崩溃过(检测标记并清除)。
// dumpDir 内只保留最近若干份 dump。非 Windows 平台为空实现,恒返回 false。
bool installCrashHandler(const QString& dumpDir);

} // namespace pixora
