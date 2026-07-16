#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Relationship.hpp"
#include "core/TableMetadata.hpp"

namespace dbscanner::core {

enum class ScanStatus { Completed, Interrupted, Failed };

std::string toString(ScanStatus status);

struct ConsistencyFinding {
    Relationship relationship;
    int64_t orphanCount = 0;
    // Each sample is one violating row's key values, in childColumns order.
    std::vector<std::vector<std::string>> sampleKeys;
};

struct DependencyCycleFinding {
    std::vector<std::string> tablesInCycle;
};

struct ScanResult {
    std::string schemaOwner;
    std::string startedAtIso;
    std::string finishedAtIso;  // empty if not finished
    ScanStatus status = ScanStatus::Interrupted;
    std::vector<TableInfo> tables;
    std::vector<Relationship> relationships;
    std::vector<ConsistencyFinding> consistencyFindings;
    std::vector<DependencyCycleFinding> cycles;
};

void to_json(nlohmann::json& j, const ConsistencyFinding& f);
void from_json(const nlohmann::json& j, ConsistencyFinding& f);

void to_json(nlohmann::json& j, const DependencyCycleFinding& c);
void from_json(const nlohmann::json& j, DependencyCycleFinding& c);

void to_json(nlohmann::json& j, const ScanResult& r);
void from_json(const nlohmann::json& j, ScanResult& r);

}  // namespace dbscanner::core
