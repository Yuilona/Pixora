#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

namespace pixora {

// 长截图控制条:状态点 + 已拼高度(大号数字)+ 提示文字 | 自动滚动开关 |
// 撤销 | 贴图 另存 取消 [复制]。提示区定宽可折行,状态变化时整条不跳宽。
// 不抢焦点,不遮挡捕获区域(置于区域下方,放不下则上方)。
class ScrollCaptureBar : public QWidget {
    Q_OBJECT
public:
    // 状态色调:中性(等待/说明)、顺利(拼接中)、需注意(对不齐等)
    enum class Tone { Neutral, Good, Warning };

    ScrollCaptureBar(const QRect& regionGlobal, const QRect& virtualBounds,
                     bool autoModeAvailable);

    void setProgress(int logicalHeight);
    void setStatus(const QString& text, Tone tone = Tone::Neutral);
    void setAutoChecked(bool checked);
    void setUndoEnabled(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override; // 自绘圆角卡片 + 投影

signals:
    void autoToggled(bool enabled);
    void finishRequested();      // 复制(默认出口)
    void finishPinRequested();   // 贴图
    void finishSaveRequested();  // 另存
    void cancelRequested();
    void undoRequested();        // 撤销最后一段拼接

private:
    QWidget* dot_ = nullptr;
    QLabel* height_ = nullptr;
    QLabel* status_ = nullptr;
    QToolButton* autoBtn_ = nullptr;
    QToolButton* undoBtn_ = nullptr;
};

} // namespace pixora
