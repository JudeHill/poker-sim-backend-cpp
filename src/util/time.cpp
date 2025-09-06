#include "util/time.h"
#include <chrono>
#include <ctime>
#include <cstdio>

std::string nowIso() {
    using namespace std::chrono;
    auto tp = time_point_cast<milliseconds>(system_clock::now());
    auto t = system_clock::to_time_t(tp);

    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);

    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count() % 1000;
    char out[40];
    std::snprintf(out, sizeof(out), "%s.%03lldZ", buf, static_cast<long long>(ms));
    return std::string(out);
}
