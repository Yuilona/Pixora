#pragma once

#include "core/annotate/AnnotationDocument.h"
#include "core/annotate/AnnotationTypes.h"
#include "core/capture/DesktopSnapshot.h"
#include "platform/interface/PlatformTypes.h"

#include <QObject>
#include <QPoint>
#include <QRect>

#include <memory>
#include <optional>
#include <vector>

namespace pixora {

// 一次截图会话:持有冻结快照与选区状态,UI 层(遮罩窗)读取快照、
// 回写选区。M1 覆盖 Selecting→输出;Adjusting/Annotating 状态随
// 后续里程碑加入(见 ARCHITECTURE §5.1 状态机)。
class SnipSession : public QObject {
    Q_OBJECT
public:
    explicit SnipSession(DesktopSnapshot snapshot, QObject* parent = nullptr);

    const DesktopSnapshot& snapshot() const { return snapshot_; }

    QRect selection() const { return selection_; } // 全局逻辑坐标
    bool hasSelection() const;
    void setSelection(const QRect& rect);

    // 窗口吸附:候选列表按 Z 序(最顶在前),悬停命中取首个包含点的窗口。
    void setWindowCandidates(std::vector<WindowInfo> windows);
    void updateHover(const QPoint& globalLogical);
    QRect hoverRect() const { return hover_; }

    void confirm();       // 确认选区 → 复制输出
    void requestSave();   // 确认选区 → 另存输出
    void requestPin();    // 确认选区 → 贴图输出
    void requestScroll(); // 确认选区 → 转入长截图(滚动拼接)
    void cancel();

    // UI 在一次拖拽交互(框选/吸附/调整)结束时调用,工具条据此显示。
    void notifyInteractionFinished();

    // —— 标注(Annotating)——
    AnnotationDocument& document() { return document_; }
    std::optional<AnnotationTool> activeTool() const { return tool_; }
    void setActiveTool(std::optional<AnnotationTool> tool);
    StrokeStyle strokeStyle() const { return style_; }
    void setStrokeStyle(const StrokeStyle& style) { style_ = style; }

    // 进行中的标注(按下→拖动→松开);跨屏遮罩窗共享预览。
    void beginAnnotation(const QPoint& globalPos);
    void updateAnnotation(const QPoint& globalPos);
    void endAnnotation(); // 提交至撤销栈
    const AnnotationItem* pendingAnnotation() const { return pending_.get(); }

    // —— 已有条目的选中/移动/删除(无激活工具时)——
    int selectedAnnotation() const { return selectedAnnotation_; } // -1 = 未选中
    bool selectAnnotationAt(const QPoint& globalPos); // 顶层优先;未命中则清除选中
    void clearAnnotationSelection();
    void moveSelectedAnnotation(const QPoint& delta);          // 拖动中实时应用
    void commitSelectedAnnotationMove(const QPoint& totalDelta); // 松开后入撤销栈
    void deleteSelectedAnnotation();

signals:
    void selectionChanged(const QRect& rect);
    void hoverChanged(const QRect& rect);
    void interactionFinished();
    void confirmed(const QRect& rect);
    void saveRequested(const QRect& rect);
    void pinRequested(const QRect& rect);
    void scrollRequested(const QRect& rect);
    void cancelled();
    void activeToolChanged();
    void annotationsChanged(); // 文档或进行中标注变化

private:
    DesktopSnapshot snapshot_;
    QRect selection_;
    QRect hover_;
    std::vector<WindowInfo> candidates_;

    AnnotationDocument document_;
    std::optional<AnnotationTool> tool_;
    StrokeStyle style_;
    std::unique_ptr<AnnotationItem> pending_;
    int selectedAnnotation_ = -1;
};

} // namespace pixora
