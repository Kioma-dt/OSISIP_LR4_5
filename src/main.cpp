#include "config.hpp"
#include "logger.hpp"
#include "simulation.hpp"
#include <iostream>
#include <exception>

int main(int argc, char* argv[])
{
    try {
        const Config c = Parse(argc, argv);
        LogInit("PHILOSOPHER", 1, c.quiet);
        const Result r = Simulate(c);

        for (int i = 0; i < c.n; ++i) {
            const auto& s = r.workers[i];
            PrintRaw("P%d TID=%lu DONE=%d TIMEOUTS=%d "
                    "AVG_WAIT_MS=%.3f MAX_WAIT_MS=%.3f\n",
                    i, static_cast<unsigned long>(s.tid),
                    s.done, s.timeouts,
                    s.samples ? s.sum / s.samples : 0, s.maximum);
        }

        PrintRaw("ELAPSED_MS=%.3f VIOLATIONS=%d ERRORS=%d DEADLINE=%d\n",
                r.ms, r.violations, r.errors, r.deadline ? 1 : 0);

        if (r.errors || g_log.failed) return 1;
        return r.deadline ? 2 : 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}