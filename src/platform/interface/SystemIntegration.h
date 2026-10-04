#pragma once

#include <QString>

class QWindow;

namespace pixora {

// 开机自启项携带的启动参数:主程序据此识别"登录时被系统拉起"(推迟联网等启动动作)
inline constexpr char kAutoStartArgument[] = "--autostart";

// 开机自启的实际状态(不止"有/无":设置面板与启动对账要区分处理)
enum class AutoStartState {
    Off,              // 无自启项
    Enabled,          // 自启项指向当前可执行文件且生效
    DisabledBySystem, // 指向当前可执行文件,但被任务管理器/系统设置禁用
    StalePath,        // 指向已不存在的文件(便携版被移动 / 卸载残留)
    OtherCopy,        // 指向另一个仍存在的可执行文件(安装版、开发构建等)
};

struct AutoStartStatus {
    AutoStartState state = AutoStartState::Off;
    QString target; // 自启项指向的可执行文件路径(Off 时为空)
};

// PAL 纯虚接口:系统集成杂项(见 ARCHITECTURE §4.1)。
// requestPermissions(macOS TCC 引导)随 M4。
class ISystemIntegration {
public:
    virtual ~ISystemIntegration() = default;

    virtual void setClickThrough(QWindow* window, bool enabled) = 0;
    // 开启 = 以当前可执行文件写入自启项并解除系统侧禁用;关闭 = 删除自启项。
    // 返回是否确实落地为期望状态:写入后 sync+回读校验,被安全软件/权限挡下时返回 false。
    virtual bool setAutoStart(bool enabled) = 0;
    virtual AutoStartStatus autoStartStatus() const = 0;

    bool isAutoStartEnabled() const {
        return autoStartStatus().state == AutoStartState::Enabled;
    }
};

} // namespace pixora
