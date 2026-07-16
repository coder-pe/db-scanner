#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "config/CliArgs.hpp"

namespace dbscanner::config {

struct ConfigError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Config {
    std::string connectString;
    std::string username;
    std::string schemaOwner;
    std::string outputDir = "dbscanner-out";
    int threads = 4;
    bool resume = false;
    std::string logLevel = "info";
    int sampleSize = 20;
    std::vector<std::string> excludeTablePatterns;
    bool inferRelationships = true;

    // Merges an optional JSON config file (if args.configPath is set) with CLI
    // flags -- CLI flags always win over file values -- and validates that all
    // required fields (connectString, username) ended up set. Does NOT resolve
    // the password; call resolvePassword() separately.
    static Config fromCliArgs(const CliArgs& args);
};

// Resolves the Oracle password from DBSCANNER_ORACLE_PWD if set, otherwise
// prompts interactively with echo disabled. Throws ConfigError if unset and
// stdin is not a TTY (can't prompt non-interactively).
std::string resolvePassword();

}  // namespace dbscanner::config
