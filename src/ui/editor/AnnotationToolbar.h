#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QWidget>

#include <vector>

class QToolButton;

namespace pixora {

class SnipSession;

// 截图工具条:选区交互结束后出现在选区下方。
// 工具选择/样式 → SnipSession;撤销重做 → AnnotationDocument 的撤销栈;
// 复制/另存/贴图/取消 → 会话出口(见 ARCHITECTURE §5.2/§8)。
// 悬浮提示自管理(进入即显,无延迟):本窗为无焦点工具窗
// (WindowDoesNotAcceptFocus),Qt 默认 tooltip 通道在此场景下不可靠。
class AnnotationToolbar : public QWidget {
    Q_OBJECT
public:
    explicit AnnotationToolbar(SnipSession& session);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reposition();
    void chooseTool(AnnotationTool tool, bool checked);

    SnipSession& session_;
    std::vector<QToolButton*> toolButtons_;
    std::vector<QToolButton*> colorButtons_;
    QToolButton* widthButton_ = nullptr;
    int widthIndex_ = 1; // 0细/1中/2粗
};

} // namespace pixora
