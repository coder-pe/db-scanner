#include "config/Config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace dbscanner::config {

namespace {

nlohmann::json loadJsonFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw ConfigError("could not open config file: " + path);
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw ConfigError("invalid JSON in config file '" + path + "': " + e.what());
    }
    return j;
}

template <typename T>
void applyIfPresent(const nlohmann::json& j, const char* key, T& target) {
    if (j.contains(key) && !j.at(key).is_null()) {
        target = j.at(key).get<T>();
    }
}

bool isStdinTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string promptHiddenPassword() {
    std::cout << "Oracle password: " << std::flush;
    std::string password;

#ifdef _WIN32
    // Minimal fallback; interactive Windows support is not the primary target.
    std::getline(std::cin, password);
#else
    termios oldTerm{};
    tcgetattr(STDIN_FILENO, &oldTerm);
    termios newTerm = oldTerm;
    newTerm.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &newTerm);

    std::getline(std::cin, password);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
    std::cout << std::endl;
#endif

    return password;
}

}  // namespace

Config Config::fromCliArgs(const CliArgs& args) {
    Config cfg;

    if (args.configPath.has_value()) {
        const nlohmann::json j = loadJsonFile(*args.configPath);
        applyIfPresent(j, "connectString", cfg.connectString);
        applyIfPresent(j, "username", cfg.username);
        applyIfPresent(j, "schemaOwner", cfg.schemaOwner);
        applyIfPresent(j, "outputDir", cfg.outputDir);
        applyIfPresent(j, "threads", cfg.threads);
        applyIfPresent(j, "logLevel", cfg.logLevel);
        applyIfPresent(j, "sampleSize", cfg.sampleSize);
        applyIfPresent(j, "excludeTablePatterns", cfg.excludeTablePatterns);
        applyIfPresent(j, "inferRelationships", cfg.inferRelationships);
    }

    if (args.connectString.has_value()) cfg.connectString = *args.connectString;
    if (args.username.has_value()) cfg.username = *args.username;
    if (args.schemaOwner.has_value()) cfg.schemaOwner = *args.schemaOwner;
    if (args.outputDir.has_value()) cfg.outputDir = *args.outputDir;
    if (args.threads.has_value()) cfg.threads = *args.threads;
    if (args.logLevel.has_value()) cfg.logLevel = *args.logLevel;
    if (args.sampleSize.has_value()) cfg.sampleSize = *args.sampleSize;
    for (const auto& pattern : args.excludeTablePatterns) {
        cfg.excludeTablePatterns.push_back(pattern);
    }
    if (args.noInfer) cfg.inferRelationships = false;
    cfg.resume = args.resume;

    if (cfg.connectString.empty()) {
        throw ConfigError("missing required connect string (--connect or config 'connectString')");
    }
    if (cfg.username.empty()) {
        throw ConfigError("missing required username (--user or config 'username')");
    }
    if (cfg.schemaOwner.empty()) {
        cfg.schemaOwner = cfg.username;
    }
    // Oracle folds unquoted identifiers to uppercase, so ALL_TABLES.OWNER etc.
    // store the owner name in uppercase; the data-dictionary queries compare
    // it with a case-sensitive bind, so normalize here. This only matters for
    // schemas created with a quoted, case-sensitive owner name (rare) -- such
    // setups aren't supported by this heuristic.
    cfg.schemaOwner = toUpper(cfg.schemaOwner);
    if (cfg.threads < 1) {
        throw ConfigError("threads must be >= 1");
    }
    if (cfg.sampleSize < 0) {
        throw ConfigError("sample-size must be >= 0");
    }

    return cfg;
}

std::string resolvePassword() {
    if (const char* env = std::getenv("DBSCANNER_ORACLE_PWD")) {
        return std::string(env);
    }
    if (!isStdinTty()) {
        throw ConfigError(
            "no password available: set DBSCANNER_ORACLE_PWD or run interactively");
    }
    return promptHiddenPassword();
}

}  // namespace dbscanner::config
