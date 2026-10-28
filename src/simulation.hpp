#pragma once
#include "config.hpp"
#include <cstdint>
#include <vector>

#pragma once
#include "config.hpp"
#include <cstdint>
#include <vector>

struct Stat {
    uint32_t tid = 0;
    int done = 0;
    int samples = 0;
    int timeouts = 0;
    double sum = 0;
    double maximum = 0;
    double aborted = 0;
};

struct Result {
    std::vector<Stat> workers;
    double ms = 0;
    int errors = 0;
    int violations = 0;
    bool deadline = false;
};

Result Simulate(const Config& config);