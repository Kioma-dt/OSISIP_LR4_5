#pragma once
#include "config.hpp"
#include <cstdint>
#include <vector>

struct Stat {
    uint32_t tid = 0;
    int done = 0;
};

struct Result {
    std::vector<Stat> workers;
    int errors = 0;
};

Result Simulate(const Config& config);