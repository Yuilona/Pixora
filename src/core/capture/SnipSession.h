#pragma once

#include "core/capture/DesktopSnapshot.h"
#include "platform/interface/PlatformTypes.h"

#include <QObject>
#include <QPoint>
#include <QRect>

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

    void confirm();     // 确认选区 → 复制输出
    void requestSave(); // 确认选区 → 另存输出
    void cancel();

signals:
    void selectionChanged(const QRect& rect);
    void hoverChanged(const QRect& rect);
    void confirmed(const QRect& rect);
    void saveRequested(const QRect& rect);
    void cancelled();

private:
    DesktopSnapshot snapshot_;
    QRect selection_;
    QRect hover_;
    std::vector<WindowInfo> candidates_;
};

} // namespace pixora
