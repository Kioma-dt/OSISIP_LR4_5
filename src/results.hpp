#pragma once
#include "simulation.hpp"
#include "logger.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

inline std::ofstream OpenCsv(const std::string& path)
{
    std::ofstream file(path, std::ios::app | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open CSV: " + path);
    file << std::fixed << std::setprecision(6);
    if (file.tellp() == std::streampos(0))
        file << "kind,run,pid,status,n,cycles,eat_ms,think_max_ms,wait_ms,limit_ms,"
                "seed,student_id,quiet,worker,tid,completed,acquisitions,timeouts,"
                "aborted_wait_ms,mean_wait_ms,max_wait_ms,elapsed_ms,ops_s,violations\n";
    return file;
}

inline void WriteResults(const Config& c, const Result& r, std::ostream& csv)
{
    const char* status = r.errors ? "ERROR" : r.deadline ? "DEADLINE" : "COMPLETED";
    PrintRaw("\nRUN=%u STATUS=%s N=%d CYCLES=%d EAT_MS=%d THINK_MAX_MS=%d "
             "WAIT_MS=%d LIMIT_MS=%d SEED=%d STUDENT_ID=%d\n",
             g_log.runId, status, c.n, c.cycles, c.eat, c.think,
             c.wait, c.limit, c.seed, c.student);
    Stat total;
    auto row = [&](const char* kind, int id, const Stat& s) {
        const double mean = s.samples ? s.sum / s.samples : 0;
        const double speed = r.ms > 0 ? s.done * 1000.0 / r.ms : 0;
        PrintRaw("%s P=%d TID=%lu DONE=%d ACQUISITIONS=%d TIMEOUTS=%d "
                 "ABORTED_WAIT_MS=%.3f AVG_WAIT_MS=%.3f MAX_WAIT_MS=%.3f\n",
                 kind, id, static_cast<unsigned long>(s.tid), s.done,
                 s.samples, s.timeouts, s.aborted, mean, s.maximum);
        csv << kind << ',' << g_log.runId << ',' << GetCurrentProcessId() << ','
            << status << ',' << c.n << ',' << c.cycles << ',' << c.eat << ','
            << c.think << ',' << c.wait << ',' << c.limit << ',' << c.seed << ','
            << c.student << ',' << c.quiet << ',' << id << ',' << s.tid << ','
            << s.done << ',' << s.samples << ',' << s.timeouts << ',' << s.aborted << ','
            << mean << ',' << s.maximum << ',' << r.ms << ',' << speed << ','
            << r.violations << '\n';
    };
    for (int i = 0; i < c.n; ++i) {
        const auto& s = r.workers[i];
        row("WORKER", i, s);
        total.done += s.done; total.samples += s.samples;
        total.timeouts += s.timeouts; total.sum += s.sum;
        total.aborted += s.aborted;
        total.maximum = std::max(total.maximum, s.maximum);
    }
    row("SUMMARY", -1, total);
    PrintRaw("ELAPSED_MS=%.3f OPS_S=%.3f EXPECTED=%d VIOLATIONS=%d\n",
             r.ms, r.ms > 0 ? total.done * 1000.0 / r.ms : 0,
             c.n * c.cycles, r.violations);
    csv.flush();
    if (!csv) throw std::runtime_error("CSV write failed");
}