#include "platform/windows/WinGlobalHotkey.h"

#include <QCoreApplication>

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

namespace {

// QKeySequence → RegisterHotKey 的修饰符+虚拟键。覆盖 F1-F24、字母、数字、
// 小键盘、导航/编辑键(含 PrtSc)与 OEM 标点。OEM 系列按 US 物理布局映射;
// shift 才能打出的符号(~ ! @ ? …)归一到对应物理键并补 MOD_SHIFT,
// 这样设置里直接录入 "Ctrl+~" 之类也能注册。非 US 布局的标点键 VK 可能不同。
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

    // 小键盘:Qt 用 KeypadModifier 区分主键盘与小键盘的同名键位。
    if (mods & Qt::KeypadModifier) {
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            outVk = VK_NUMPAD0 + static_cast<UINT>(key - Qt::Key_0);
            return true;
        }
        switch (key) {
        case Qt::Key_Slash: outVk = VK_DIVIDE; return true;
        case Qt::Key_Asterisk: outVk = VK_MULTIPLY; return true;
        case Qt::Key_Minus: outVk = VK_SUBTRACT; return true;
        case Qt::Key_Plus: outVk = VK_ADD; return true;
        case Qt::Key_Period: outVk = VK_DECIMAL; return true;
        case Qt::Key_Enter: outVk = VK_RETURN; return true;
        default: break;
        }
    }

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

    switch (key) {
    // —— 导航 / 编辑 / 锁定键 ——
    case Qt::Key_Print: outVk = VK_SNAPSHOT; return true; // PrtSc:截图工具经典键
    case Qt::Key_Insert: outVk = VK_INSERT; return true;
    case Qt::Key_Delete: outVk = VK_DELETE; return true;
    case Qt::Key_Home: outVk = VK_HOME; return true;
    case Qt::Key_End: outVk = VK_END; return true;
    case Qt::Key_PageUp: outVk = VK_PRIOR; return true;
    case Qt::Key_PageDown: outVk = VK_NEXT; return true;
    case Qt::Key_Up: outVk = VK_UP; return true;
    case Qt::Key_Down: outVk = VK_DOWN; return true;
    case Qt::Key_Left: outVk = VK_LEFT; return true;
    case Qt::Key_Right: outVk = VK_RIGHT; return true;
    case Qt::Key_Space: outVk = VK_SPACE; return true;
    case Qt::Key_Tab: outVk = VK_TAB; return true;
    case Qt::Key_Return:
    case Qt::Key_Enter: outVk = VK_RETURN; return true;
    case Qt::Key_Escape: outVk = VK_ESCAPE; return true;
    case Qt::Key_Backspace: outVk = VK_BACK; return true;
    case Qt::Key_Pause: outVk = VK_PAUSE; return true;
    case Qt::Key_ScrollLock: outVk = VK_SCROLL; return true;
    case Qt::Key_CapsLock: outVk = VK_CAPITAL; return true;

    // —— OEM 标点(US 物理布局)——
    case Qt::Key_Semicolon: outVk = VK_OEM_1; return true;
    case Qt::Key_Equal: outVk = VK_OEM_PLUS; return true;
    case Qt::Key_Comma: outVk = VK_OEM_COMMA; return true;
    case Qt::Key_Minus: outVk = VK_OEM_MINUS; return true;
    case Qt::Key_Period: outVk = VK_OEM_PERIOD; return true;
    case Qt::Key_Slash: outVk = VK_OEM_2; return true;
    case Qt::Key_QuoteLeft: outVk = VK_OEM_3; return true;
    case Qt::Key_BracketLeft: outVk = VK_OEM_4; return true;
    case Qt::Key_Backslash: outVk = VK_OEM_5; return true;
    case Qt::Key_BracketRight: outVk = VK_OEM_6; return true;
    case Qt::Key_Apostrophe: outVk = VK_OEM_7; return true;

    // —— 需 shift 的符号:归一到物理键并强制补 SHIFT ——
    case Qt::Key_Colon: outVk = VK_OEM_1; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Plus: outVk = VK_OEM_PLUS; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Less: outVk = VK_OEM_COMMA; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Underscore: outVk = VK_OEM_MINUS; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Greater: outVk = VK_OEM_PERIOD; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Question: outVk = VK_OEM_2; outMods |= MOD_SHIFT; return true;
    case Qt::Key_AsciiTilde: outVk = VK_OEM_3; outMods |= MOD_SHIFT; return true;
    case Qt::Key_BraceLeft: outVk = VK_OEM_4; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Bar: outVk = VK_OEM_5; outMods |= MOD_SHIFT; return true;
    case Qt::Key_BraceRight: outVk = VK_OEM_6; outMods |= MOD_SHIFT; return true;
    case Qt::Key_QuoteDbl: outVk = VK_OEM_7; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Exclam: outVk = '1'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_At: outVk = '2'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_NumberSign: outVk = '3'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Dollar: outVk = '4'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Percent: outVk = '5'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_AsciiCircum: outVk = '6'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Ampersand: outVk = '7'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_Asterisk: outVk = '8'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_ParenLeft: outVk = '9'; outMods |= MOD_SHIFT; return true;
    case Qt::Key_ParenRight: outVk = '0'; outMods |= MOD_SHIFT; return true;

    default: break;
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
