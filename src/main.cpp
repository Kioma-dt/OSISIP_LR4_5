#include "config.hpp"
#include "logger.hpp"
#include "simulation.hpp"
#include "results.hpp"
#include <iostream>
#include <exception>

int main(int argc, char* argv[])
{
    try {
        const Config c = Parse(argc, argv);
        auto csv = OpenCsv(c.csv);
        LogInit("PHILOSOPHER", 1, c.quiet);
        const Result r = Simulate(c);
        WriteResults(c, r, csv);
        if (r.errors || g_log.failed) return 1;
        return r.deadline ? 2 : 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}