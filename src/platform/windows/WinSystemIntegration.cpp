#include "platform/windows/WinSystemIntegration.h"

#include "platform/windows/AutoStartPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QWindow>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

namespace {
const QString kRunKey = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kRunValue = QStringLiteral("Pixora");
} // namespace

void WinSystemIntegration::setClickThrough(QWindow* window, bool enabled) {
    if (!window) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    LONG exStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (enabled) {
        exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    ::SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);
}

bool WinSystemIntegration::setAutoStart(bool enabled) {
    const QString exe = QCoreApplication::applicationFilePath();
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled) {
        run.setValue(kRunValue, QStringLiteral("\"%1\"").arg(
                                    QDir::toNativeSeparators(exe)));
    } else {
        run.remove(kRunValue);
    }
    // sync() 落盘并回读注册表实际状态:能反映权限失败(status)与同步发生的
    // 外部还原(安全软件)。异步还原(写入后才被改回)此处测不到,由下次启动
    // 的对账兜底;返回值如实反映"现在是否已是期望状态"。
    run.sync();
    if (run.status() != QSettings::NoError) {
        return false;
    }
    const QString stored = run.value(kRunValue).toString();
    return enabled ? autostart::sameExecutablePath(stored, exe) : stored.isEmpty();
}

bool WinSystemIntegration::isAutoStartEnabled() const {
    QSettings run(kRunKey, QSettings::NativeFormat);
    return autostart::sameExecutablePath(run.value(kRunValue).toString(),
                                         QCoreApplication::applicationFilePath());
}

} // namespace pixora
