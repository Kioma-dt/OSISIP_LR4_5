#include "simulation.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <utility>
#include <random>

namespace {
void Require(bool ok, const char* api)
{
    if (!ok) {
        ReportWinError(api, GetLastError());
        throw std::runtime_error(api);
    }
}

struct Fork {
    CRITICAL_SECTION cs;
    std::atomic<int> owner{-1};
    Fork() { Require(InitializeCriticalSectionEx(&cs, 0, 0), "InitializeCriticalSectionEx"); }
    ~Fork() { DeleteCriticalSection(&cs); }
};

struct State;
struct Worker { State* state = nullptr; int id = 0; };

struct State {
    const Config& c;
    Result result;
    std::unique_ptr<Fork[]> forks;
    Worker args[64]{};
    HANDLE threads[64]{};
    HANDLE stop = nullptr;
    int count = 0;
    std::atomic<int> errors{0};
    std::atomic<int> violations{0};
    std::atomic<bool> deadline{false};

    explicit State(const Config& config) : c(config), forks(new Fork[c.n])
    {
        result.workers.resize(c.n);
        for (int i = 0; i < c.n; ++i) args[i] = {this, i};
        stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Require(stop != nullptr, "CreateEventW");
    }

    void Cancel()
    {
        if (stop && !SetEvent(stop)) {
            const DWORD error = GetLastError();
            ++errors;
            ReportWinError("SetEvent", error);
        }
    }

    void Fail(const char* api)
    {
        const DWORD error = GetLastError();
        ++errors;
        ReportWinError(api, error);
        Cancel();
    }

    void Check(bool ok, int id, int resource = -1)
    {
        if (ok) return;
        ++errors; ++violations;
        LogEvent("INVARIANT", "P%d RESOURCE=%d", id, resource);
        Cancel();
    }

    bool Stopped()
    {
        if (Elapsed() >= c.limit) deadline = true;
        if (errors || deadline || g_log.failed) { Cancel(); return true; }
        return false;
    }

    bool Pause(DWORD ms)
    {
        if (Stopped()) return false;
        const DWORD timeout = static_cast<DWORD>(
            std::min<double>(ms, std::max(1.0, c.limit - Elapsed())));
        const DWORD r = WaitForSingleObject(stop, timeout);
        if (r == WAIT_FAILED) { Fail("WaitForSingleObject"); return false; }
        return r == WAIT_TIMEOUT && !Stopped();
    }

    void Finish()
    {
        if (!stop) return;
        if (count && WaitForMultipleObjects(count, threads, TRUE, INFINITE)
                     != WAIT_OBJECT_0) {
            ReportWinError("WaitForMultipleObjects", GetLastError());
            std::abort();
        }
        for (int i = 0; i < count; ++i)
            if (!CloseHandle(threads[i])) Fail("CloseHandle");
        count = 0;
        for (int i = 0; i < c.n; ++i)
            Check(forks[i].owner == -1, -1, i);
        HANDLE h = std::exchange(stop, nullptr);
        if (!CloseHandle(h)) Fail("CloseHandle");
    }

    ~State() { if (stop) { Cancel(); Finish(); } }
};

struct Pair {
    State& s;
    int id;
    int resources[2];
    int held = 0;
    Pair(State& state, int philosopher) : s(state), id(philosopher)
    {
        resources[0] = std::min(id, (id + 1) % s.c.n);
        resources[1] = std::max(id, (id + 1) % s.c.n);
    }
    Pair(const Pair&) = delete;
    Pair& operator=(const Pair&) = delete;

    bool Take(double until)
    {
        while (held < 2 && !s.Stopped() && Elapsed() < until) {
            const int k = resources[held];
            if (TryEnterCriticalSection(&s.forks[k].cs)) {
                ++held;
                s.Check(s.forks[k].owner.exchange(id) == -1, id, k);
                LogEvent("ACQUIRED", "P%d RESOURCE=%d", id, k);
            }
            else if (!s.Pause(1)) return false;
        }
        if (held != 2 || s.Stopped()) return false;
        s.Check(s.forks[resources[0]].owner == id &&
                s.forks[resources[1]].owner == id, id);
        return !s.Stopped();
    }

    ~Pair()
    {
        while (held) {
            const int k = resources[--held];
            s.Check(s.forks[k].owner.exchange(-1) == id, id, k);
            LogEvent("RELEASED", "P%d RESOURCE=%d", id, k);
            LeaveCriticalSection(&s.forks[k].cs);
        }
    }
};

DWORD WINAPI Work(LPVOID data)
{
    const auto& w = *static_cast<Worker*>(data);
    auto& s = *w.state;
    auto& stat = s.result.workers[w.id];
    stat.tid = GetCurrentThreadId();

    std::mt19937 rng(static_cast<uint32_t>(s.c.seed) + w.id);
    std::uniform_int_distribution<int> thinking(0, s.c.think);

    for (int cycle = 0; cycle < s.c.cycles && !s.Stopped(); ++cycle) {
        LogEvent("THINKING", "P%d", w.id);

        if (!s.Pause(thinking(rng))) break;

        LogEvent("WAITING", "P%d RESOURCES=%d,%d", w.id, w.id, (w.id + 1) % s.c.n);
        const double began = Elapsed();
        bool got = false, ate = false;
        while (!s.Stopped()) {
            {
                Pair pair(s, w.id);
                if (pair.Take(Elapsed() + s.c.wait)) {
                    got = true;
                    const double wait = Elapsed() - began;
                    ++stat.samples;
                    stat.sum += wait;
                    stat.maximum = std::max(stat.maximum, wait);
                    LogEvent("EATING", "P%d", w.id);
                    if ((ate = s.Pause(s.c.eat))) ++stat.done;
                }
            }
            if (got || s.Stopped()) break;
            ++stat.timeouts;
            LogEvent("TIMEOUT", "P%d RESOURCES=%d,%d", w.id, w.id, (w.id + 1) % s.c.n);
            if (!s.Pause(1)) break;
        }
        if (!got) stat.aborted += Elapsed() - began;
        if (!ate) break;
    }
    LogEvent(stat.done == s.c.cycles ? "FINISHED" : "STOPPED",
             "P%d COMPLETED=%d", w.id, stat.done);
    return 0;
}
}

Result Simulate(const Config& config)
{
    State s(config);
    for (int i = 0; i < config.n; ++i) {
        HANDLE h = CreateThread(nullptr, 0, Work, &s.args[i], 0, nullptr);
        if (!h) { s.Fail("CreateThread"); break; }
        s.threads[s.count++] = h;
    }
    s.Finish();
    if (!s.errors && !s.deadline && !g_log.failed)
        for (int i = 0; i < config.n; ++i)
            s.Check(s.result.workers[i].done == config.cycles, i);
    s.result.ms = Elapsed();
    s.result.errors = s.errors.load() + (g_log.failed ? 1 : 0);
    s.result.violations = s.violations.load();
    s.result.deadline = s.deadline.load();
    return std::move(s.result);
}