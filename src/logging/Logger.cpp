#include "logging/Logger.hpp"

#include <filesystem>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace dbscanner::logging {

namespace {
spdlog::level::level_enum parseLevel(const std::string& level) {
    const auto parsed = spdlog::level::from_str(level);
    // spdlog::level::from_str() silently returns 'off' for unrecognized strings;
    // only accept 'off' when the user actually asked for it.
    if (parsed == spdlog::level::off && level != "off") {
        return spdlog::level::info;
    }
    return parsed;
}
}  // namespace

void initLogger(const LoggerOptions& options) {
    namespace fs = std::filesystem;
    const fs::path logDir = fs::path(options.outputDir) / "logs";
    std::error_code ec;
    fs::create_directories(logDir, ec);

    std::vector<spdlog::sink_ptr> sinks;
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    sinks.push_back(consoleSink);

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        (logDir / "dbscanner.log").string(), options.maxFileSizeBytes, options.maxFiles);
    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
    sinks.push_back(fileSink);

    auto logger = std::make_shared<spdlog::logger>("dbscanner", sinks.begin(), sinks.end());
    logger->set_level(parseLevel(options.level));
    logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger);
}

}  // namespace dbscanner::logging
