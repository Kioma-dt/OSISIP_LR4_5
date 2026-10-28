#include "simulation.hpp"
#include "logger.hpp"
#include <cstdlib>

namespace {
struct Worker {
    const Config* config;
    Stat* stat;
    int id;
};

DWORD WINAPI Work(LPVOID data)
{
    const auto& w = *static_cast<Worker*>(data);
    w.stat->tid = GetCurrentThreadId();

    for (int i = 0; i < w.config->cycles; ++i) {
        LogEvent("THINKING", "P%d", w.id);
        Sleep(w.config->think);
        ++w.stat->done;
    }

    LogEvent("FINISHED", "P%d", w.id);
    return 0;
}
}

Result Simulate(const Config& config)
{
    Result result;
    result.workers.resize(config.n);
    Worker args[64]{};
    HANDLE threads[64]{};
    int count = 0;

    for (int i = 0; i < config.n; ++i) {
        args[i] = {&config, &result.workers[i], i};
        HANDLE h = CreateThread(nullptr, 0, Work, &args[i], 0, nullptr);
        if (!h) {
            ReportWinError("CreateThread", GetLastError());
            ++result.errors;
            break;
        }
        threads[count++] = h;
    }

    if (count && WaitForMultipleObjects(count, threads, TRUE, INFINITE)
                 != WAIT_OBJECT_0) {
        ReportWinError("WaitForMultipleObjects", GetLastError());
        std::abort();
    }

    for (int i = 0; i < count; ++i) {
        if (!CloseHandle(threads[i])) {
            ReportWinError("CloseHandle", GetLastError());
            ++result.errors;
        }
    }

    return result;
}