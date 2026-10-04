#pragma once

#include <QString>

namespace pixora {

// 开发用:把截图遮罩、标注工具栏、放大镜、长截图控制条、通知卡离屏渲染到
// 一张对照图(浅色/深色两类底图各一半,检验投影与对比度)。
// 所有窗口以 WA_DontShowOnScreen 渲染,不出现在屏幕上、不注册热键、
// 不冻结桌面——视觉迭代时可随时对照。入口:pixora --ui-gallery [输出路径]
bool renderUiGallery(const QString& outPath);

} // namespace pixora
