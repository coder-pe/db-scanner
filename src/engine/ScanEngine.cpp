#include "engine/ScanEngine.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <future>
#include <regex>
#include <set>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "core/TimeUtil.hpp"
#include "engine/ThreadPool.hpp"
#include "relations/DependencyGraph.hpp"
#include "relations/RelationshipInference.hpp"

namespace dbscanner::engine {

namespace {

using core::nowIso;

std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return result;
}

bool matchesGlob(const std::string& pattern, const std::string& text) {
    std::string regexStr;
    regexStr.reserve(pattern.size() * 2);
    for (char c : pattern) {
        if (c == '*') {
            regexStr += ".*";
        } else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            regexStr += c;
        } else {
            regexStr += '\\';
            regexStr += c;
        }
    }
    try {
        return std::regex_match(toUpper(text), std::regex(toUpper(regexStr)));
    } catch (const std::regex_error&) {
        return false;
    }
}

}  // namespace

ScanEngine::ScanEngine(db::ISchemaSource& schemaSource, db::IConsistencyChecker& consistencyChecker,
                        checkpoint::CheckpointStore& checkpointStore, ScanEngineOptions options)
    : schemaSource_(schemaSource),
      consistencyChecker_(consistencyChecker),
      checkpointStore_(checkpointStore),
      options_(std::move(options)) {}

bool ScanEngine::isExcluded(const std::string& tableName) const {
    for (const auto& pattern : options_.excludeTablePatterns) {
        if (matchesGlob(pattern, tableName)) return true;
    }
    return false;
}

void ScanEngine::reportProgress(const std::string& phase, std::size_t current, std::size_t total,
                                 const std::string& itemName) {
    if (!progressCallback_) return;
    std::lock_guard<std::mutex> lock(resultMutex_);
    progressCallback_(phase, current, total, itemName);
}

void ScanEngine::runDiscoveryPhase(ShutdownController& shutdown, core::ScanResult& result) {
    std::vector<std::string> tableNames;
    for (const auto& name : schemaSource_.listTableNames()) {
        if (!isExcluded(name)) tableNames.push_back(name);
    }

    const std::size_t total = tableNames.size();
    std::atomic<std::size_t> completedCount{0};
    ThreadPool pool(options_.threads);
    std::vector<std::future<void>> futures;

    for (const auto& tableName : tableNames) {
        if (checkpointStore_.getTableStatus(tableName) == checkpoint::UnitStatus::Done) {
            if (auto payload = checkpointStore_.getTablePayload(tableName)) {
                try {
                    core::TableInfo info = nlohmann::json::parse(*payload).get<core::TableInfo>();
                    {
                        std::lock_guard<std::mutex> lock(resultMutex_);
                        result.tables.push_back(std::move(info));
                    }
                    const std::size_t done = ++completedCount;
                    reportProgress("discovery", done, total, tableName + " (cached)");
                    continue;
                } catch (const std::exception&) {
                    checkpointStore_.markTableStatus(tableName, checkpoint::UnitStatus::Pending);
                }
            }
        }

        futures.push_back(pool.submit([this, &shutdown, &result, tableName, &completedCount, total] {
            if (shutdown.stopRequested()) return;
            checkpointStore_.markTableStatus(tableName, checkpoint::UnitStatus::InProgress);
            try {
                core::TableInfo info = schemaSource_.readTable(tableName);
                const nlohmann::json payload = info;
                checkpointStore_.markTableDone(tableName, payload.dump());
                {
                    std::lock_guard<std::mutex> lock(resultMutex_);
                    result.tables.push_back(std::move(info));
                }
            } catch (const std::exception& e) {
                checkpointStore_.markTableStatus(tableName, checkpoint::UnitStatus::Error, e.what());
                spdlog::error("discovery failed for table {}: {}", tableName, e.what());
            }
            const std::size_t done = ++completedCount;
            reportProgress("discovery", done, total, tableName);
        }));
    }

    for (auto& future : futures) future.get();
}

void ScanEngine::runConsistencyPhase(ShutdownController& shutdown, core::ScanResult& result,
                                      const std::vector<core::Relationship>& relationshipsToCheck) {
    const std::size_t total = relationshipsToCheck.size();
    std::atomic<std::size_t> completedCount{0};
    ThreadPool pool(options_.threads);
    std::vector<std::future<void>> futures;

    for (const auto& rel : relationshipsToCheck) {
        const std::string relId = rel.id();

        if (checkpointStore_.getRelationStatus(relId) == checkpoint::UnitStatus::Done) {
            if (auto payload = checkpointStore_.getRelationPayload(relId)) {
                try {
                    core::ConsistencyFinding finding =
                        nlohmann::json::parse(*payload).get<core::ConsistencyFinding>();
                    {
                        std::lock_guard<std::mutex> lock(resultMutex_);
                        result.consistencyFindings.push_back(std::move(finding));
                    }
                    const std::size_t done = ++completedCount;
                    reportProgress("consistency", done, total, relId + " (cached)");
                    continue;
                } catch (const std::exception&) {
                    checkpointStore_.markRelationStatus(relId, checkpoint::UnitStatus::Pending);
                }
            }
        }

        futures.push_back(pool.submit([this, &shutdown, &result, rel, relId, &completedCount, total] {
            if (shutdown.stopRequested()) return;
            checkpointStore_.markRelationStatus(relId, checkpoint::UnitStatus::InProgress);
            try {
                core::ConsistencyFinding finding = consistencyChecker_.checkOrphans(rel, options_.sampleSize);
                const nlohmann::json payload = finding;
                checkpointStore_.markRelationDone(relId, payload.dump());
                {
                    std::lock_guard<std::mutex> lock(resultMutex_);
                    result.consistencyFindings.push_back(std::move(finding));
                }
            } catch (const std::exception& e) {
                checkpointStore_.markRelationStatus(relId, checkpoint::UnitStatus::Error, e.what());
                spdlog::error("consistency check failed for {}: {}", relId, e.what());
            }
            const std::size_t done = ++completedCount;
            reportProgress("consistency", done, total, relId);
        }));
    }

    for (auto& future : futures) future.get();
}

core::ScanResult ScanEngine::run(ShutdownController& shutdown) {
    core::ScanResult result;
    result.schemaOwner = options_.schemaOwner;
    result.startedAtIso = nowIso();
    result.status = core::ScanStatus::Interrupted;

    checkpointStore_.resetInProgressToPending();
    if (checkpointStore_.isFreshRun()) {
        checkpointStore_.setRunInfo({{"schemaOwner", options_.schemaOwner}, {"startedAtIso", result.startedAtIso}});
    } else {
        const auto info = checkpointStore_.getRunInfo();
        if (const auto it = info.find("startedAtIso"); it != info.end()) {
            result.startedAtIso = it->second;
        }
        spdlog::info("resuming previous run (started {})", result.startedAtIso);
    }

    runDiscoveryPhase(shutdown, result);
    if (shutdown.stopRequested()) {
        result.finishedAtIso = nowIso();
        return result;
    }

    std::vector<std::string> includedTableNames;
    includedTableNames.reserve(result.tables.size());
    for (const auto& table : result.tables) includedTableNames.push_back(table.name);
    const std::set<std::string> includedSet(includedTableNames.begin(), includedTableNames.end());

    std::vector<core::Relationship> declared;
    for (auto& rel : schemaSource_.listDeclaredForeignKeys()) {
        if (includedSet.count(rel.parentTable) && includedSet.count(rel.childTable)) {
            declared.push_back(std::move(rel));
        }
    }

    result.relationships = declared;
    if (options_.inferRelationships) {
        auto inferred = relations::inferRelationships(result.tables, declared);
        result.relationships.insert(result.relationships.end(), inferred.begin(), inferred.end());
    }

    auto graph = relations::DependencyGraph::fromRelationships(includedTableNames, result.relationships);
    result.cycles = graph.detectCycles();

    if (shutdown.stopRequested()) {
        result.finishedAtIso = nowIso();
        return result;
    }

    runConsistencyPhase(shutdown, result, result.relationships);

    result.finishedAtIso = nowIso();
    result.status = shutdown.stopRequested() ? core::ScanStatus::Interrupted : core::ScanStatus::Completed;
    checkpointStore_.setRunInfo({{"finishedAtIso", result.finishedAtIso}, {"status", toString(result.status)}});
    return result;
}

}  // namespace dbscanner::engine
