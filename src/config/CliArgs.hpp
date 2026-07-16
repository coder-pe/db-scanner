#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dbscanner::config {

struct CliParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct CliArgs {
    std::optional<std::string> connectString;  // easy-connect or TNS alias, e.g. host:port/service
    std::optional<std::string> username;
    std::optional<std::string> schemaOwner;   // defaults to username if omitted
    std::optional<std::string> configPath;
    std::optional<std::string> outputDir;
    std::optional<int> threads;
    bool resume = false;
    std::optional<std::string> logLevel;
    std::optional<int> sampleSize;
    std::vector<std::string> excludeTablePatterns;
    bool noInfer = false;  // disable relationship inference, declared FKs only
    bool showHelp = false;
    bool showVersion = false;
};

// Parses argv[1..argc). Throws CliParseError on malformed input (unknown flag,
// missing value for a flag that requires one, non-numeric value where an int
// is expected).
CliArgs parseCliArgs(int argc, char** argv);

std::string usageText();
std::string versionText();

}  // namespace dbscanner::config
