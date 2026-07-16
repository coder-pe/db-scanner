#include <cstdlib>
#include <iostream>

#include <spdlog/spdlog.h>

#include "checkpoint/CheckpointStore.hpp"
#include "config/CliArgs.hpp"
#include "config/Config.hpp"
#include "db/OracleConnectionPool.hpp"
#include "db/OracleConsistencyChecker.hpp"
#include "db/OracleSchemaReader.hpp"
#include "engine/ScanEngine.hpp"
#include "engine/ShutdownController.hpp"
#include "logging/Logger.hpp"
#include "report/ConsoleReporter.hpp"
#include "report/JsonReportWriter.hpp"

using namespace dbscanner;

namespace {

int runScan(const config::Config& cfg, const std::string& password) {
    engine::ShutdownController shutdownController;
    shutdownController.install();

    checkpoint::CheckpointStore checkpointStore(cfg.outputDir + "/checkpoint.db");

    const bool hasIncompleteRun = !checkpointStore.isFreshRun();
    if (hasIncompleteRun && !cfg.resume) {
        spdlog::error(
            "an incomplete run already exists in '{}'. Pass --resume to continue it, or use a "
            "different --output directory.",
            cfg.outputDir);
        return 1;
    }
    if (cfg.resume && !hasIncompleteRun) {
        spdlog::warn("--resume was given but no previous run was found in '{}'; starting fresh.",
                     cfg.outputDir);
    }

    db::OracleConnectionPool pool(cfg.connectString, cfg.username, password, cfg.threads);
    db::OracleSchemaReader schemaReader(pool, cfg.schemaOwner);
    db::OracleConsistencyChecker consistencyChecker(pool);

    engine::ScanEngineOptions options;
    options.schemaOwner = cfg.schemaOwner;
    options.threads = cfg.threads;
    options.sampleSize = cfg.sampleSize;
    options.inferRelationships = cfg.inferRelationships;
    options.excludeTablePatterns = cfg.excludeTablePatterns;

    engine::ScanEngine scanEngine(schemaReader, consistencyChecker, checkpointStore, options);
    scanEngine.setProgressCallback(
        [](const std::string& phase, std::size_t current, std::size_t total, const std::string& item) {
            report::printProgress(phase, current, total, item);
        });

    spdlog::info("starting scan of schema '{}' with {} worker thread(s)", cfg.schemaOwner, cfg.threads);
    const core::ScanResult result = scanEngine.run(shutdownController);
    report::endProgress();

    report::writeAllReports(result, cfg.outputDir);
    report::printSummary(result);

    if (result.status == core::ScanStatus::Interrupted) {
        spdlog::warn("scan interrupted; re-run with --resume to continue");
        return 130;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    config::CliArgs args;
    try {
        args = config::parseCliArgs(argc, argv);
    } catch (const config::CliParseError& e) {
        std::cerr << "error: " << e.what() << "\n\n" << config::usageText();
        return 1;
    }

    if (args.showHelp) {
        std::cout << config::usageText();
        return 0;
    }
    if (args.showVersion) {
        std::cout << config::versionText() << "\n";
        return 0;
    }

    config::Config cfg;
    try {
        cfg = config::Config::fromCliArgs(args);
    } catch (const config::ConfigError& e) {
        std::cerr << "error: " << e.what() << "\n\n" << config::usageText();
        return 1;
    }

    logging::initLogger({cfg.outputDir, cfg.logLevel});

    std::string password;
    try {
        password = config::resolvePassword();
    } catch (const config::ConfigError& e) {
        spdlog::error("{}", e.what());
        return 1;
    }

    try {
        return runScan(cfg, password);
    } catch (const db::OracleError& e) {
        spdlog::error("{}", e.what());
        return 1;
    } catch (const std::exception& e) {
        spdlog::error("unexpected error: {}", e.what());
        return 1;
    }
}
