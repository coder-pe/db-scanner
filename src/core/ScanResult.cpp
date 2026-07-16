#include "core/ScanResult.hpp"

namespace dbscanner::core {

std::string toString(ScanStatus status) {
    switch (status) {
        case ScanStatus::Completed:
            return "Completed";
        case ScanStatus::Interrupted:
            return "Interrupted";
        case ScanStatus::Failed:
            return "Failed";
    }
    return "Unknown";
}

void to_json(nlohmann::json& j, const ConsistencyFinding& f) {
    j = nlohmann::json{
        {"relationship", f.relationship},
        {"orphanCount", f.orphanCount},
        {"sampleKeys", f.sampleKeys},
    };
}

void from_json(const nlohmann::json& j, ConsistencyFinding& f) {
    j.at("relationship").get_to(f.relationship);
    j.at("orphanCount").get_to(f.orphanCount);
    j.at("sampleKeys").get_to(f.sampleKeys);
}

void to_json(nlohmann::json& j, const DependencyCycleFinding& c) {
    j = nlohmann::json{{"tablesInCycle", c.tablesInCycle}};
}

void from_json(const nlohmann::json& j, DependencyCycleFinding& c) {
    j.at("tablesInCycle").get_to(c.tablesInCycle);
}

void to_json(nlohmann::json& j, const ScanResult& r) {
    j = nlohmann::json{
        {"schemaOwner", r.schemaOwner},
        {"startedAtIso", r.startedAtIso},
        {"finishedAtIso", r.finishedAtIso},
        {"status", toString(r.status)},
        {"tables", r.tables},
        {"relationships", r.relationships},
        {"consistencyFindings", r.consistencyFindings},
        {"cycles", r.cycles},
    };
}

void from_json(const nlohmann::json& j, ScanResult& r) {
    j.at("schemaOwner").get_to(r.schemaOwner);
    j.at("startedAtIso").get_to(r.startedAtIso);
    j.at("finishedAtIso").get_to(r.finishedAtIso);
    const std::string status = j.at("status").get<std::string>();
    r.status = status == "Completed"    ? ScanStatus::Completed
               : status == "Interrupted" ? ScanStatus::Interrupted
                                          : ScanStatus::Failed;
    j.at("tables").get_to(r.tables);
    j.at("relationships").get_to(r.relationships);
    j.at("consistencyFindings").get_to(r.consistencyFindings);
    j.at("cycles").get_to(r.cycles);
}

}  // namespace dbscanner::core
