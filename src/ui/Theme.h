#pragma once

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRect>
#include <QString>

// 全局 UI 主题:双色系分工 + 主题蓝点缀(与 logo 蓝同族)。
//
// - 悬浮 HUD(截图工具栏/长截图控制条/通知卡/拼接预览/贴图框):
//   蓝灰系暗色,浮在任意屏幕内容上都稳得住;
// - 文档型窗口(设置/历史等带标题栏的常规窗口):白色系,
//   与系统浅色标题栏自然衔接(见 appStyleSheet)。
// 圆角体系:输入框/按钮 6px,卡片/菜单/工具栏 8px,通知卡 10px。
//
// 所有窗体"外壳"色集中在此,禁止在窗体里硬编码外壳色。
// 不在此列的色彩:标注六色画笔(内容色)、遮罩/放大镜 HUD(刻意纯黑高对比)、
// 长截图区域框(刻意橙色与选区蓝区分)。
namespace pixora::theme {

// —— 色板 ——
inline QColor windowBg() { return {0x1E, 0x21, 0x26}; }     // 窗口底,最深
inline QColor surface() { return {0x28, 0x2C, 0x33}; }      // 卡片/工具条
inline QColor surfaceHover() { return {0x34, 0x39, 0x43}; } // 悬停态
inline QColor inputBg() { return {0x1A, 0x1D, 0x22}; }      // 输入框深井
inline QColor border() { return {0x3A, 0x3F, 0x49}; }       // 控件描边
inline QColor hairline() { return {255, 255, 255, 24}; }    // 卡片细描边
inline QColor text() { return {0xF2, 0xF3, 0xF5}; }
inline QColor textDim() { return {0xC8, 0xCC, 0xD2}; }
inline QColor textFaint() { return {0x8B, 0x91, 0x9C}; }    // 占位/禁用
inline QColor accent() { return {45, 124, 246}; }           // 主题蓝
inline QColor accentHover() { return {0x4A, 0x90, 0xF8}; }
inline QColor accentPressed() { return {0x25, 0x66, 0xCC}; }
inline QColor danger() { return {0xE5, 0x48, 0x4D}; }       // 错误/冲突提示

// 圆角悬浮卡片底(工具栏/控制条/通知卡共用):
// QSS 样式背景在 WA_TranslucentBackground 顶层窗上不可靠,统一自绘。
inline void paintCard(QPainter& p, const QRect& rect, qreal radius,
                      int bgAlpha = 255) {
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath card;
    card.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), radius,
                        radius);
    QColor bg = surface();
    bg.setAlpha(bgAlpha);
    p.fillPath(card, bg);
    p.setPen(QPen(hairline(), 1));
    p.drawPath(card);
}

// 截图工具栏 / 长截图控制条共用的"工具条壳"样式(底块由 paintCard 自绘,
// 此处只管子控件;窗口须设 WA_TranslucentBackground)
inline QString chromeStyleSheet(int fontPx, int padV, int padH) {
    return QStringLiteral(
               "QLabel { color: %1; font-size: %3px; padding: 0 6px;"
               "  background: transparent; }"
               "QToolButton { color: %1; background: transparent; border: none;"
               "  border-radius: 5px; padding: %4px %5px; font-size: %3px; }"
               "QToolButton:hover { background: %2; }"
               "QToolButton:checked { background: %6; color: white; }")
        .arg(textDim().name(), surfaceHover().name())
        .arg(fontPx)
        .arg(padV)
        .arg(padH)
        .arg(accent().name());
}

// 全应用样式表:设置/历史等文档型窗口的白色系(色值只在本函数出现,
// 仍算集中管理)。悬浮 HUD 自绘窗体有局部样式,不受影响——
// 本表刻意不写全局 QWidget/QToolButton 规则。
inline QString appStyleSheet() {
    QString qss = QStringLiteral(R"(
QDialog, QMainWindow { background: #F5F6F8; }

QLabel { color: #3A4150; background: transparent; }

/* —— 卡片化分组:白卡浮于浅灰底 —— */
QGroupBox {
    background: #FFFFFF;
    border: 1px solid #E3E6EB;
    border-radius: 8px;
    margin-top: 14px;
    padding: 14px 6px 6px 6px;
    font-weight: 600;
    color: #1F2329;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    top: 3px;
    padding: 0 6px;
    color: #1F2329;
}

/* —— 输入类:白底 + 聚焦主题蓝描边 —— */
QLineEdit, QKeySequenceEdit, QSpinBox, QComboBox {
    background: #FFFFFF;
    color: #1F2329;
    border: 1px solid #D5D9E0;
    border-radius: 6px;
    padding: 5px 8px;
    selection-background-color: @accent;
    selection-color: white;
}
QLineEdit:focus, QKeySequenceEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: @accent;
}
QLineEdit:disabled, QKeySequenceEdit:disabled, QSpinBox:disabled,
QComboBox:disabled {
    color: #9AA3B0;
    background: #F0F1F4;
}

QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #FFFFFF;
    color: #1F2329;
    border: 1px solid #E3E6EB;
    border-radius: 6px;
    selection-background-color: @accent;
    selection-color: white;
    outline: 0;
    padding: 4px;
}

QSpinBox::up-button, QSpinBox::down-button {
    background: transparent;
    border: none;
    width: 16px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
    background: #EDEFF3;
    border-radius: 3px;
}

/* —— 按钮:次级浅灰 / [primary] 实心主题蓝 —— */
QPushButton {
    background: #EEF0F4;
    color: #1F2329;
    border: 1px solid #D5D9E0;
    border-radius: 6px;
    padding: 6px 18px;
}
QPushButton:hover { background: #E4E7EC; }
QPushButton:pressed { background: #D8DCE3; }
QPushButton:disabled { color: #9AA3B0; background: #F0F1F4; }
QPushButton[primary="true"] {
    background: @accent;
    color: white;
    border: none;
    font-weight: 600;
}
QPushButton[primary="true"]:hover { background: @accentHover; }
QPushButton[primary="true"]:pressed { background: @accentPressed; }

/* —— 复选框 —— */
QCheckBox { color: #3A4150; spacing: 8px; background: transparent; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border-radius: 4px;
    border: 1px solid #C9CED8;
    background: #FFFFFF;
}
QCheckBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked {
    background: @accent;
    border-color: @accent;
    image: url(:/icons/check-14.png);
}
QCheckBox:disabled { color: #9AA3B0; }

/* —— 菜单(托盘/右键) —— */
QMenu {
    background: #FFFFFF;
    color: #1F2329;
    border: 1px solid #E3E6EB;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item { padding: 6px 28px 6px 12px; border-radius: 5px; }
QMenu::item:selected { background: @accent; color: white; }
QMenu::item:disabled { color: #9AA3B0; }
QMenu::separator { height: 1px; background: #E9EBEF; margin: 5px 8px; }

/* —— 列表(历史窗) —— */
QListWidget {
    background: #F5F6F8;
    border: none;
    outline: 0;
}
QListWidget::item { color: #3A4150; border-radius: 6px; padding: 4px; }
QListWidget::item:hover { background: #EAEDF1; }
QListWidget::item:selected {
    background: rgba(45, 124, 246, 0.16);
    color: #1F2329;
}

/* —— 滚动条:细圆条,无箭头 —— */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical {
    background: #C5CAD3; border-radius: 4px; min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #AEB5C0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal {
    background: #C5CAD3; border-radius: 4px; min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #AEB5C0; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip {
    background: #FFFFFF;
    color: #1F2329;
    border: 1px solid #E3E6EB;
    padding: 4px 8px;
}
)");
    qss.replace(QLatin1String("@accentHover"), accentHover().name());
    qss.replace(QLatin1String("@accentPressed"), accentPressed().name());
    qss.replace(QLatin1String("@accent"), accent().name());
    return qss;
}

} // namespace pixora::theme
