#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointer>

#include <vector>

namespace pixora {

class ISystemIntegration;
class PinWindow;

// 贴图编排:截图转贴图 / 剪贴板贴图(F3)/ 关闭所有贴图(托盘)。
// 贴图窗自管理生命周期(关闭即销毁),本服务弱引用跟踪;
// 完整 PinManager 模型(分组/序列化/恢复)随 M5 落到 core/pin(见 §5.4)。
class PinService : public QObject {
    Q_OBJECT
public:
    explicit PinService(ISystemIntegration* system, QObject* parent = nullptr);

    void pinImage(const QImage& image, const QPoint& topLeftLogical);

    // 剪贴板有图像则贴出(位置取光标附近),返回是否成功。
    bool pinFromClipboard();

    // 关闭全部贴图(含已开启点击穿透、无法直接交互的)。
    void closeAll();

private:
    ISystemIntegration* system_;
    std::vector<QPointer<PinWindow>> pins_;
};

} // namespace pixora
