#include "config.hpp"
#include <stdexcept>

static int Number(const std::string& text, int low, int high)
{
    std::size_t used = 0;
    const long long n = std::stoll(text, &used);
    if (used != text.size() || n < low || n > high)
        throw std::runtime_error("Invalid value: " + text);
    return static_cast<int>(n);
}

Config Parse(int argc, char* argv[])
{
    Config c;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        
        if (key == "--quiet") { c.quiet = true; continue; }

        if (++i == argc)
            throw std::runtime_error("Missing value: " + key);
        const std::string value = argv[i];

        if (key == "--n") c.n = Number(value, 2, 64);
        else if (key == "--cycles") c.cycles = Number(value, 1, 1000000);
        else if (key == "--eat") c.eat = Number(value, 0, 60000);
        else if (key == "--think") c.think = Number(value, 0, 60000);
        else throw std::runtime_error("Unknown option: " + key);
    }
    return c;
}