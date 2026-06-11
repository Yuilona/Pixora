#pragma once

#include <QImage>
#include <QPixmap>
#include <QWidget>

namespace pixora {

// 长截图实时预览条:竖窄条悬浮在捕获区域旁,实时显示已拼内容的
// 尾部(底部对齐,跟随增长),错位当场可见。完全不接收输入。
// 位置只在区域左右两侧选,绝不与捕获区域重叠(否则会被抓进成图)。
class ScrollPreview : public QWidget {
    Q_OBJECT
public:
    // 两侧都放不下时 fitsBeside() 为假,调用方应放弃创建
    static bool fitsBeside(const QRect& regionGlobal, const QRect& virtualBounds);

    ScrollPreview(const QRect& regionGlobal, const QRect& virtualBounds);

    // 需要的尾部行数(物理像素):内容区高度按画布宽折算
    int tailRowsFor(int canvasWidthPhysical) const;

    void updateContent(const QImage& tail, int totalLogicalHeight);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap scaled_;
    int totalLogicalHeight_ = 0;
};

} // namespace pixora
