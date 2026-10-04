#pragma once

#include "core/annotate/AnnotationTypes.h"

#include <QWidget>

#include <vector>

class QAbstractButton;
class QToolButton;
class QVBoxLayout;

namespace pixora {

class SnipSession;

// 截图工具条:选区交互结束后出现在选区下方,两层卡片——
// - 主栏:标注工具 | 撤销重做 | 文字/翻译/长截图 | 贴图 另存 取消 [复制];
// - 情境栏:选中标注工具或已有条目时才出现,放颜色与大小(马赛克/模糊只有大小)。
// 样式不常驻主栏,主栏宽度因此收窄一半,小选区下也不至于远超选区。
// 工具选择/样式 → SnipSession;撤销重做 → AnnotationDocument 的撤销栈;
// 复制/另存/贴图/取消 → 会话出口(见 ARCHITECTURE §5.2/§8)。
class AnnotationToolbar : public QWidget {
    Q_OBJECT
public:
    explicit AnnotationToolbar(SnipSession& session);

protected:
    void paintEvent(QPaintEvent* event) override; // 每层各画一张带投影的圆角卡片

private:
    QWidget* buildMainRow();
    QWidget* buildContextRow();
    void reposition();
    // 工具选中态、情境栏显隐与取值都以会话为准(Esc 退工具、点选条目等
    // 不经工具条的变化也能同步)
    void syncFromSession();

    SnipSession& session_;
    QVBoxLayout* rows_ = nullptr;
    QWidget* mainRow_ = nullptr;
    QWidget* contextRow_ = nullptr;
    QWidget* colorGroup_ = nullptr;
    std::vector<QToolButton*> toolButtons_;
    std::vector<QAbstractButton*> colorButtons_;
    std::vector<QAbstractButton*> sizeButtons_;
};

} // namespace pixora
