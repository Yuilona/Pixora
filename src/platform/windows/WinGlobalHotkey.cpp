#include "platform/windows/WinGlobalHotkey.h"

#include <QCoreApplication>

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

namespace {

// QKeySequence → RegisterHotKey 的修饰符+虚拟键。M0 覆盖常用键
// (F1-F24、字母、数字),完整映射随热键自定义 UI 一起扩展。
bool toWinHotkey(const QKeySequence& seq, UINT& outMods, UINT& outVk) {
    if (seq.isEmpty()) {
        return false;
    }
    const QKeyCombination combo = seq[0];

    outMods = MOD_NOREPEAT;
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    if (mods & Qt::ControlModifier) outMods |= MOD_CONTROL;
    if (mods & Qt::AltModifier) outMods |= MOD_ALT;
    if (mods & Qt::ShiftModifier) outMods |= MOD_SHIFT;
    if (mods & Qt::MetaModifier) outMods |= MOD_WIN;

    const int key = combo.key();
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        outVk = VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
        return true;
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        outVk = 'A' + static_cast<UINT>(key - Qt::Key_A);
        return true;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        outVk = '0' + static_cast<UINT>(key - Qt::Key_0);
        return true;
    }
    return false;
}

} // namespace

WinGlobalHotkey::WinGlobalHotkey(QObject* parent) : IGlobalHotkey(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WinGlobalHotkey::~WinGlobalHotkey() {
    unregisterAll();
    if (auto* app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
}

bool WinGlobalHotkey::registerHotkey(HotkeyId id, const QKeySequence& seq) {
    UINT mods = 0;
    UINT vk = 0;
    if (!toWinHotkey(seq, mods, vk)) {
        spdlog::warn("unsupported hotkey sequence: {}",
                     seq.toString(QKeySequence::PortableText).toStdString());
        return false;
    }
    // hWnd 为空时 WM_HOTKEY 投递到本线程消息队列,经 nativeEventFilter 捕获。
    if (!::RegisterHotKey(nullptr, static_cast<int>(id), mods, vk)) {
        spdlog::warn("RegisterHotKey failed for {} (error {}), likely taken by another app",
                     seq.toString(QKeySequence::PortableText).toStdString(), ::GetLastError());
        return false;
    }
    registeredIds_.push_back(static_cast<int>(id));
    return true;
}

void WinGlobalHotkey::unregisterAll() {
    for (int id : registeredIds_) {
        ::UnregisterHotKey(nullptr, id);
    }
    registeredIds_.clear();
}

bool WinGlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message,
                                        qintptr* /*result*/) {
    if (eventType == "windows_generic_MSG") {
        const MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            emit activated(static_cast<HotkeyId>(msg->wParam));
        }
    }
    return false;
}

} // namespace pixora
