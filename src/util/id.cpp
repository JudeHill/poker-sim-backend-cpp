#include "util/id.h"
#include <random>

std::string randId(std::size_t n) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static const char* chars = "0123456789abcdef";
    std::uniform_int_distribution<int> dist(0, 15);

    std::string s;
    s.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        s.push_back(chars[dist(rng)]);
    }
    return s;
}
