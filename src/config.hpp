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
};

Config Parse(int argc, char* argv[]);