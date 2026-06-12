#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

// 全局 UI 主题:暗色为基调 + 主题蓝点缀(与 logo 蓝同族)。
// 所有窗体"外壳"色集中在此,禁止在窗体里硬编码外壳色。
// 不在此列的色彩:标注六色画笔(内容色)、遮罩/放大镜 HUD(刻意纯黑高对比)、
// 长截图区域框(刻意橙色与选区蓝区分)。
namespace pixora::theme {

inline QColor surface() { return {0x2B, 0x2B, 0x2B}; }      // 卡片/工具条底色
inline QColor surfaceHover() { return {0x3D, 0x3D, 0x3D}; } // 悬停态
inline QColor accent() { return {45, 124, 246}; }           // 主题蓝:选中/边框/高亮
inline QColor text() { return {0xFF, 0xFF, 0xFF}; }
inline QColor textDim() { return {0xDD, 0xDD, 0xDD}; }
inline QColor hairline() { return {255, 255, 255, 28}; }    // 卡片描边
inline QColor danger() { return {0xD9, 0x30, 0x25}; }       // 错误/冲突提示

// 截图工具栏 / 长截图控制条共用的"工具条壳"样式
inline QString chromeStyleSheet(int fontPx, int padV, int padH) {
    return QStringLiteral(
               "QWidget { background: %1; }"
               "QLabel { color: %2; font-size: %4px; padding: 0 6px; }"
               "QToolButton { color: %2; background: transparent; border: none;"
               "  padding: %5px %6px; font-size: %4px; }"
               "QToolButton:hover { background: %3; }"
               "QToolButton:checked { background: %7; color: white; }")
        .arg(surface().name(), textDim().name(), surfaceHover().name())
        .arg(fontPx)
        .arg(padV)
        .arg(padH)
        .arg(accent().name());
}

// 原生窗体(设置/历史/托盘菜单)的 Fusion 暗色调色板,与自绘 UI 同基调
inline QPalette darkPalette() {
    QPalette p;
    const QColor windowText(0xE8, 0xE8, 0xE8);
    const QColor disabledText(0x80, 0x80, 0x80);
    p.setColor(QPalette::Window, surface());
    p.setColor(QPalette::WindowText, windowText);
    p.setColor(QPalette::Base, QColor(0x20, 0x20, 0x20));
    p.setColor(QPalette::AlternateBase, QColor(0x33, 0x33, 0x33));
    p.setColor(QPalette::Text, windowText);
    p.setColor(QPalette::Button, QColor(0x36, 0x36, 0x36));
    p.setColor(QPalette::ButtonText, windowText);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent());
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, accent().lighter(125));
    p.setColor(QPalette::ToolTipBase, surface());
    p.setColor(QPalette::ToolTipText, textDim());
    p.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x8A, 0x8A));
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x50, 0x50, 0x50));
    return p;
}

} // namespace pixora::theme
