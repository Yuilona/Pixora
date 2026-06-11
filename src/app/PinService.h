#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>

namespace pixora {

// 贴图编排:截图转贴图 / 剪贴板贴图(F3)。
// 贴图窗自管理生命周期(关闭即销毁);完整 PinManager 模型
// (分组/序列化/恢复)随 M5 落到 core/pin(见 ARCHITECTURE §5.4)。
class PinService : public QObject {
    Q_OBJECT
public:
    explicit PinService(QObject* parent = nullptr);

    void pinImage(const QImage& image, const QPoint& topLeftLogical);

    // 剪贴板有图像则贴出(位置取光标附近),返回是否成功。
    bool pinFromClipboard();
};

} // namespace pixora
