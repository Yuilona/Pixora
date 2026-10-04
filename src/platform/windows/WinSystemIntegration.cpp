#include "platform/windows/WinSystemIntegration.h"

#include "platform/windows/AutoStartPath.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QWindow>

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace pixora {

namespace {
const QString kRunKey = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kRunValue = QStringLiteral("Pixora");

// 任务管理器/系统设置的启用开关(REG_BINARY)。QSettings 会把 REG_BINARY
// 当 UTF-16 字符串读,字节不可靠,故直接走 Win32 API。
constexpr wchar_t kApprovedSubKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kApprovedValue[] = L"Pixora";

QByteArray readStartupApproved() {
    DWORD size = 0;
    if (::RegGetValueW(HKEY_CURRENT_USER, kApprovedSubKey, kApprovedValue,
                       RRF_RT_REG_BINARY, nullptr, nullptr, &size) != ERROR_SUCCESS ||
        size == 0) {
        return {};
    }
    QByteArray data(static_cast<qsizetype>(size), '\0');
    if (::RegGetValueW(HKEY_CURRENT_USER, kApprovedSubKey, kApprovedValue,
                       RRF_RT_REG_BINARY, nullptr, data.data(), &size) != ERROR_SUCCESS) {
        return {};
    }
    data.truncate(static_cast<qsizetype>(size));
    return data;
}

// 删除系统侧启用/禁用记录:值不存在即视为启用(任务管理器下次会按需重建)
bool clearStartupApproved() {
    const LSTATUS rc =
        ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedSubKey, kApprovedValue);
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND;
}
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
        run.setValue(kRunValue, QStringLiteral("\"%1\" %2").arg(
                                    QDir::toNativeSeparators(exe),
                                    QLatin1String(kAutoStartArgument)));
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
    // 任务管理器里的"已禁用"记录会压过 Run 项:开启时必须清除,否则写了也不生效;
    // 关闭时顺手清理残留
    if (!clearStartupApproved()) {
        spdlog::warn("failed to clear StartupApproved entry");
    }
    const AutoStartState state = autoStartStatus().state;
    return enabled ? state == AutoStartState::Enabled : state == AutoStartState::Off;
}

AutoStartStatus WinSystemIntegration::autoStartStatus() const {
    QSettings run(kRunKey, QSettings::NativeFormat);
    const QString command = run.value(kRunValue).toString();
    const QString target = autostart::executableFromCommand(command);
    const bool targetExists = !target.isEmpty() && QFileInfo::exists(target);
    return {autostart::classify(command, readStartupApproved(),
                                QCoreApplication::applicationFilePath(), targetExists),
            QDir::toNativeSeparators(target)};
}

} // namespace pixora
