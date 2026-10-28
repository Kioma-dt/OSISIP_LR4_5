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

        for (int i = 0; i < c.n; ++i)
            PrintRaw("P%d TID=%lu DONE=%d\n", i,
                     static_cast<unsigned long>(r.workers[i].tid),
                     r.workers[i].done);

        return r.errors || g_log.failed ? 1 : 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}