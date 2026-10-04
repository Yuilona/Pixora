#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QIcon>

// 工具条图标:Fluent UI System Icons(MIT,见 resources/icons/fluent/LICENSE)
// 的 20px SVG,运行时按目标尺寸与 DPR 渲染并着色——任意缩放比都清晰。
// 常态取 HUD 图标色;禁用态 35% 透明;选中(On)态换实心(filled)造型
// 并转白,在主题蓝选中底上可读。截图工具条与长截图控制条共用。
namespace pixora::icons {

// 标注工具(带 filled 选中态)
QIcon toolIcon(AnnotationTool tool);

// 线条粗细档位:横线粗细即当前档位(程序绘制,随档位变化)
QIcon widthIcon(int width);

// 编辑动作
QIcon undoIcon();
QIcon redoIcon();

// 功能动作
QIcon ocrIcon();       // 提取文字:扫描框 + 文本行
QIcon translateIcon(); // 翻译
QIcon scrollIcon();    // 长截图:框 + 向下延伸箭头
QIcon pinIcon();       // 贴图:图钉
QIcon saveIcon();      // 另存:软盘

// 出口动作(带语义色:对勾主题蓝,叉危险红)
QIcon confirmIcon();
QIcon cancelIcon();

} // namespace pixora::icons
