#pragma once

#include "platform/interface/GlobalHotkey.h"

#include <QObject>

namespace pixora {

class SettingsService;

// 加载热键配置 → 注册到 IGlobalHotkey → 按动作路由为具体信号(见 ARCHITECTURE §6)。
// backend 为 nullptr 时(平台无实现)降级为无热键,只记日志。
class HotkeyService : public QObject {
    Q_OBJECT
public:
    HotkeyService(SettingsService& settings, IGlobalHotkey* backend,
                  QObject* parent = nullptr);

    void registerAll();

signals:
    void captureRequested();
    void scrollCaptureRequested();
    void pinRequested();

private:
    SettingsService& settings_;
    IGlobalHotkey* backend_;
};

} // namespace pixora
