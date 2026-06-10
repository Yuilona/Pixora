#pragma once

#include <QKeySequence>
#include <QObject>

namespace pixora {

// 全局热键动作标识。数值同时用作平台层注册 id(Windows: RegisterHotKey id)。
enum class HotkeyId {
    CaptureRegion = 1,
    ScrollCapture = 2,
    PinFromClipboard = 3,
};

// PAL 纯虚接口:全局热键(见 ARCHITECTURE §4.1)。
// 上层只依赖本接口,平台实现由 PlatformFactory 注入。
class IGlobalHotkey : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    virtual bool registerHotkey(HotkeyId id, const QKeySequence& seq) = 0;
    virtual void unregisterAll() = 0;

signals:
    void activated(pixora::HotkeyId id);
};

} // namespace pixora
