#pragma once

#include <QKeySequence>
#include <QObject>

namespace pixora {

// 全局热键动作标识。数值同时用作平台层注册 id(Windows: RegisterHotKey id)。
// (id=2 曾是独立的长截图热键,已并入截图工具栏入口,保留空号)
enum class HotkeyId {
    CaptureRegion = 1,
    PinFromClipboard = 3,
    RepeatLastRegion = 4, // 以上次选区直接发起截图;默认不绑定
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
