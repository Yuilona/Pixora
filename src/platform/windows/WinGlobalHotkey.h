#pragma once

#include "platform/interface/GlobalHotkey.h"

#include <QAbstractNativeEventFilter>

#include <vector>

namespace pixora {

// Windows 实现:RegisterHotKey + 原生事件过滤器捕获 WM_HOTKEY。
class WinGlobalHotkey : public IGlobalHotkey, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit WinGlobalHotkey(QObject* parent = nullptr);
    ~WinGlobalHotkey() override;

    bool registerHotkey(HotkeyId id, const QKeySequence& seq) override;
    void unregisterAll() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

private:
    std::vector<int> registeredIds_;
};

} // namespace pixora
