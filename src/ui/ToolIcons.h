#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QIcon>

// 程序绘制的工具条图标(16x16 逻辑坐标系,2x 渲染保证高 DPI 清晰)。
// 不引入图片资源;线条色与按钮文字色一致,选中态蓝底上同样可读;
// 禁用态自带 35% 透明度变体。截图工具条与长截图控制条共用。
namespace pixora::icons {

// 标注工具
QIcon toolIcon(AnnotationTool tool);

// 线条粗细档位:横线粗细即当前档位
QIcon widthIcon(int width);

// 编辑动作
QIcon undoIcon();
QIcon redoIcon();

// 功能动作
QIcon ocrIcon();       // 提取文字:四角扫描框 + T
QIcon translateIcon(); // 翻译:文/A
QIcon scrollIcon();    // 长截图:屏幕 + 下行箭头
QIcon pinIcon();       // 贴图:图钉
QIcon saveIcon();      // 另存:软盘

// 出口动作(带语义色:对勾主题蓝,叉危险红)
QIcon confirmIcon();
QIcon cancelIcon();

} // namespace pixora::icons
