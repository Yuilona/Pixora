#pragma once

namespace pixora {

// 初始化全局日志(控制台 + 文件 + Windows 调试输出)。
// 须在 QApplication 设置好 organizationName/applicationName 之后调用。
void initLogging();

} // namespace pixora
