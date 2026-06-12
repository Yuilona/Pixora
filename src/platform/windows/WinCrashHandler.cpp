#include "platform/interface/CrashHandler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dbghelp.h>

#include <string>

namespace pixora {

namespace {

constexpr int kKeepDumps = 5;

// 崩溃现场禁止堆分配/Qt 调用,路径在安装时预先准备好
std::wstring g_dumpDir;
std::wstring g_flagPath;

LONG WINAPI writeDumpAndDie(EXCEPTION_POINTERS* info) {
    wchar_t path[1024];
    ::wsprintfW(path, L"%s\\pixora-%lu-%lu.dmp", g_dumpDir.c_str(),
                ::GetCurrentProcessId(), ::GetTickCount());
    HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{::GetCurrentThreadId(), info, FALSE};
        // ScanMemory + 间接引用内存:栈可达的堆数据也带上,体积仍是 MB 级
        const auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory);
        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, type,
                            &mei, nullptr, nullptr);
        ::CloseHandle(file);
        HANDLE flag = ::CreateFileW(g_flagPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (flag != INVALID_HANDLE_VALUE) {
            ::CloseHandle(flag);
        }
    }
    return EXCEPTION_EXECUTE_HANDLER; // 直接结束进程,不弹系统错误框
}

} // namespace

bool installCrashHandler(const QString& dumpDir) {
    QDir dir(dumpDir);
    dir.mkpath(QStringLiteral("."));

    const QString flagFile = dir.filePath(QStringLiteral("pending.flag"));
    const bool crashedLastTime = QFile::exists(flagFile);
    if (crashedLastTime) {
        QFile::remove(flagFile);
    }

    const auto dumps =
        dir.entryInfoList({QStringLiteral("*.dmp")}, QDir::Files, QDir::Time);
    for (qsizetype i = kKeepDumps; i < dumps.size(); ++i) {
        QFile::remove(dumps[i].absoluteFilePath());
    }

    g_dumpDir = QDir::toNativeSeparators(dir.absolutePath()).toStdWString();
    g_flagPath = QDir::toNativeSeparators(flagFile).toStdWString();
    ::SetUnhandledExceptionFilter(writeDumpAndDie);
    spdlog::info("crash handler installed, dumps -> {}",
                 dir.absolutePath().toStdString());
    return crashedLastTime;
}

} // namespace pixora
