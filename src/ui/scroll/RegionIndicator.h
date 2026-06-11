#pragma once

#include <QWidget>

namespace pixora {

// 长截图捕获区域指示框:置顶虚线边框,完全不接收输入
// (Qt::WindowTransparentForInput),不干扰用户滚动目标窗口。
class RegionIndicator : public QWidget {
    Q_OBJECT
public:
    explicit RegionIndicator(const QRect& regionGlobal);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace pixora
