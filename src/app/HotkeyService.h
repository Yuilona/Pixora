#pragma once

#include "platform/interface/GlobalHotkey.h"

#include <QObject>
#include <QSet>

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
    void reregisterAll(); // 设置变更后重新注册

    // 最近一次 registerAll 中该热键是否注册失败(被占用/不支持的键位)
    bool failed(HotkeyId id) const { return failed_.contains(static_cast<int>(id)); }

signals:
    void captureRequested();
    void pinRequested();
    void repeatLastRegionRequested();
    // action 为中文动作名("截图"/"贴图"),供 UI 直接拼装提示文案
    void registrationFailed(const QString& action, const QKeySequence& seq);

private:
    SettingsService& settings_;
    IGlobalHotkey* backend_;
    QSet<int> failed_;
};

} // namespace pixora
