#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <vector>

namespace pixora {

class ISystemIntegration;
class PinWindow;

// 贴图编排:截图转贴图 / 剪贴板贴图(F3)/ 关闭所有贴图(托盘)。
// 贴图窗自管理生命周期(关闭即销毁),本服务弱引用跟踪。
//
// 持久化:每张贴图存为 <AppData>/pins/<id>.png + pins.json 清单
// (位置/缩放/透明度/DPR)。状态变化防抖落盘;用户主动关闭即从
// 清单移除;程序退出时保留 → restorePins() 下次启动恢复。
class PinService : public QObject {
    Q_OBJECT
public:
    explicit PinService(ISystemIntegration* system, QObject* parent = nullptr);

    // 返回新建的贴图窗(图像为空返回 nullptr);窗口自管理生命周期,
    // 调用方如需后续操作请用 QPointer 弱引用。
    PinWindow* pinImage(const QImage& image, const QPoint& topLeftLogical);

    // 剪贴板有图像则贴出(位置取光标附近),返回是否成功。
    bool pinFromClipboard();

    // 关闭全部贴图(含已开启点击穿透、无法直接交互的)。
    void closeAll();

    // 启动时恢复上次会话留下的贴图,返回恢复数量。
    int restorePins();

private:
    struct Tracked {
        QString id;
        QPointer<PinWindow> win;
    };

    QString storageDir() const;
    void attach(const QString& id, PinWindow* pin);
    void saveManifest();

    ISystemIntegration* system_;
    std::vector<Tracked> pins_;
    QTimer saveTimer_; // 拖动/缩放高频触发 → 防抖落盘
};

} // namespace pixora
