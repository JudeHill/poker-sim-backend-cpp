#pragma once
#include <string>

// Generate a random lowercase hex string of length n (default 24).
// Uses thread-local Mersenne Twister for RNG.
std::string randId(std::size_t n = 24);
