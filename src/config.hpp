#pragma once
#include <string>

struct Config {
    int n = 5;
    int cycles = 100;
    int eat = 5;
    int think = 5;
    bool quiet = false;
};

Config Parse(int argc, char* argv[]);