#include "platform/windows/WinSystemIntegration.h"

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

void WinSystemIntegration::setAutoStart(bool enabled) {
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled) {
        run.setValue(kRunValue,
                     QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(
                         QCoreApplication::applicationFilePath())));
    } else {
        run.remove(kRunValue);
    }
}

bool WinSystemIntegration::isAutoStartEnabled() const {
    QSettings run(kRunKey, QSettings::NativeFormat);
    return run.contains(kRunValue);
}

} // namespace pixora
