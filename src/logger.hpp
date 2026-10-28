#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

struct LoggerState {
    CRITICAL_SECTION cs;
    std::string role;
    uint32_t runId = 0;
    std::atomic<bool> quiet{false}, failed{false};
    std::chrono::steady_clock::time_point started;
    LoggerState() { InitializeCriticalSection(&cs); }
    ~LoggerState() { DeleteCriticalSection(&cs); }
};
inline LoggerState g_log;

inline double Elapsed()
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - g_log.started).count();
}

inline void LogInit(const std::string& role, uint32_t runId, bool quiet)
{
    g_log.role = role;
    g_log.runId = runId;
    g_log.quiet = quiet;
    g_log.failed = false;
    g_log.started = std::chrono::steady_clock::now();
}

inline void WriteStdout(const char* text)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) { g_log.failed = true; return; }
    DWORD offset = 0, size = static_cast<DWORD>(std::strlen(text));
    while (offset < size) {
        DWORD written = 0;
        if (!WriteFile(h, text + offset, size - offset, &written, nullptr)
            || written == 0) { g_log.failed = true; return; }
        offset += written;
    }
}

inline void PrintRaw(const char* fmt, ...)
{
    char text[1400]{};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_log.cs);
    WriteStdout(text);
    LeaveCriticalSection(&g_log.cs);
}

inline void LogEvent(const char* kind, const char* fmt, ...)
{
    if (g_log.quiet && std::strcmp(kind, "ERROR") &&
        std::strcmp(kind, "INVARIANT")) return;
    char message[1024]{}, line[1400]{};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_log.cs);
    snprintf(line, sizeof(line),
        "[%08llu ms] RUN_ID=%u PID=%lu TID=%lu %s %s %s\n",
        static_cast<unsigned long long>(Elapsed()), g_log.runId,
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        g_log.role.c_str(), kind, message);
    WriteStdout(line);
    LeaveCriticalSection(&g_log.cs);
}

inline void ReportWinError(const char* function, DWORD error)
{
    LogEvent("ERROR", "WinAPI=%s CODE=%lu", function,
            static_cast<unsigned long>(error));
}