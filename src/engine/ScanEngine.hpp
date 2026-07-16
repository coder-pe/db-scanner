#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "checkpoint/CheckpointStore.hpp"
#include "core/ScanResult.hpp"
#include "db/IConsistencyChecker.hpp"
#include "db/ISchemaSource.hpp"
#include "engine/ShutdownController.hpp"

namespace dbscanner::engine {

struct ScanEngineOptions {
    std::string schemaOwner;
    int threads = 4;
    int sampleSize = 20;
    bool inferRelationships = true;
    std::vector<std::string> excludeTablePatterns;  // '*' glob, matched case-insensitively
};

// Orchestrates the full scan: Discovery (table structure) -> Relations
// (declared FK collection + name-based inference + cycle detection) ->
// Consistency (orphan checks). Depends only on the ISchemaSource /
// IConsistencyChecker interfaces, so it can be exercised in unit tests with
// fake implementations and needs no live Oracle connection or OCCI headers
// to build.
class ScanEngine {
public:
    ScanEngine(db::ISchemaSource& schemaSource, db::IConsistencyChecker& consistencyChecker,
               checkpoint::CheckpointStore& checkpointStore, ScanEngineOptions options);

    // Progress callback invoked from worker threads: (phase, current, total, itemName).
    using ProgressCallback = std::function<void(const std::string&, std::size_t, std::size_t,
                                                 const std::string&)>;
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = std::move(callback); }

    core::ScanResult run(ShutdownController& shutdown);

private:
    bool isExcluded(const std::string& tableName) const;

    void runDiscoveryPhase(ShutdownController& shutdown, core::ScanResult& result);
    void runConsistencyPhase(ShutdownController& shutdown, core::ScanResult& result,
                              const std::vector<core::Relationship>& relationshipsToCheck);
    void reportProgress(const std::string& phase, std::size_t current, std::size_t total,
                         const std::string& itemName);

    db::ISchemaSource& schemaSource_;
    db::IConsistencyChecker& consistencyChecker_;
    checkpoint::CheckpointStore& checkpointStore_;
    ScanEngineOptions options_;
    ProgressCallback progressCallback_;
    std::mutex resultMutex_;
};

}  // namespace dbscanner::engine
