#include "core/TimeUtil.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace dbscanner::core {

std::string nowIso() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&nowTimeT, &tm);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

}  // namespace dbscanner::core
