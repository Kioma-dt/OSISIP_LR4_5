#pragma once
#include <string>

struct Config {
    int n = 5;
    int cycles = 100;
    int eat = 5;
    int think = 5;
    bool quiet = false;
    int wait = 1000;
    int limit = 600000;
    int seed = 1000;
    int student = -1;
    std::string csv = "data\\results.csv";
};

Config Parse(int argc, char* argv[]);