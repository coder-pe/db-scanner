#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace dbscanner::logging {

struct LoggerOptions {
    std::string outputDir;       // logs written to <outputDir>/logs/dbscanner.log
    std::string level = "info";  // trace|debug|info|warn|error|critical|off
    std::size_t maxFileSizeBytes = 10 * 1024 * 1024;
    std::size_t maxFiles = 5;
};

// Initializes the global default spdlog logger with a console sink and a
// rotating file sink under <outputDir>/logs/. Safe to call once at startup.
void initLogger(const LoggerOptions& options);

}  // namespace dbscanner::logging
