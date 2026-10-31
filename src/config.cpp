#include "config.hpp"
#include <stdexcept>

static int Number(const std::string& text, int low, int high)
{
    std::size_t used = 0;
    const int n = std::stoi(text, &used);
    if (used != text.size() || n < low || n > high)
        throw std::runtime_error("Invalid value: " + text);
    return n;
}

Config Parse(int argc, char* argv[])
{
    Config c;
    bool cyclesGiven = false, seedGiven = false;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--quiet") { c.quiet = true; continue; }
        if (++i == argc) throw std::runtime_error("Missing value: " + key);
        const std::string value = argv[i];

        if (key == "--n") c.n = Number(value, 2, 64);
        else if (key == "--cycles") {
            c.cycles = Number(value, 1, 1000000);
            cyclesGiven = true;
        }
        else if (key == "--eat") c.eat = Number(value, 0, 60000);
        else if (key == "--think") c.think = Number(value, 0, 60000);
        else if (key == "--wait") c.wait = Number(value, 1, 60000);
        else if (key == "--limit") c.limit = Number(value, 1, 3600000);
        else if (key == "--seed") {
            c.seed = Number(value, 0, 2147483647);
            seedGiven = true;
        }
        else if (key == "--student") c.student = Number(value, 0, 999);
        else if (key == "--runs") c.runs = Number(value, 1, 100);
        else if (key == "--csv" && !value.empty()) c.csv = value;
        else throw std::runtime_error("Unknown option or empty value: " + key);
    }
    if (c.student >= 0) {
        if (cyclesGiven || seedGiven)
            throw std::runtime_error("--student conflicts with --cycles/--seed");
        c.cycles = 100 + c.student * 5;
        c.seed = 1000 + c.student * 97;
    }
    return c;
}