#include "config.hpp"
#include "logger.hpp"
#include "simulation.hpp"
#include "results.hpp"
#include <iostream>
#include <exception>


//g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp src/config.cpp src/simulation.cpp -o build/philosophers.exe
int main(int argc, char* argv[])
{
    try {
        const Config c = Parse(argc, argv);
        auto csv = OpenCsv(c.csv);
        for (int run = 1; run <= c.runs; ++run) {
            LogInit("PHILOSOPHER", run, c.quiet);
            const Result r = Simulate(c);
            WriteResults(c, r, csv);
            if (r.errors || g_log.failed) return 1;
            if (r.deadline) return 2;
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}