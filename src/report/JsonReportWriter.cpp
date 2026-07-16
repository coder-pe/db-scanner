#include "report/JsonReportWriter.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace dbscanner::report {

namespace {

void writeJsonFile(const nlohmann::json& j, const std::string& outputDir, const std::string& fileName) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(outputDir, ec);

    const fs::path path = fs::path(outputDir) / fileName;
    const fs::path tmpPath = fs::path(outputDir) / (fileName + ".tmp");

    {
        std::ofstream out(tmpPath);
        if (!out) {
            throw std::runtime_error("could not open report file for writing: " + tmpPath.string());
        }
        out << j.dump(2);
    }
    fs::rename(tmpPath, path, ec);
    if (ec) {
        throw std::runtime_error("could not finalize report file: " + path.string() + ": " + ec.message());
    }
}

}  // namespace

void writeSchemaReport(const core::ScanResult& result, const std::string& outputDir) {
    nlohmann::json j;
    j["schemaOwner"] = result.schemaOwner;
    j["startedAtIso"] = result.startedAtIso;
    j["finishedAtIso"] = result.finishedAtIso;
    j["status"] = toString(result.status);
    j["tables"] = result.tables;
    j["relationships"] = result.relationships;
    j["dependencyCycles"] = result.cycles;
    writeJsonFile(j, outputDir, "schema.json");
}

void writeConsistencyReport(const core::ScanResult& result, const std::string& outputDir) {
    int64_t violatingRelationships = 0;
    int64_t totalOrphans = 0;
    for (const auto& finding : result.consistencyFindings) {
        if (finding.orphanCount > 0) {
            ++violatingRelationships;
            totalOrphans += finding.orphanCount;
        }
    }

    nlohmann::json j;
    j["schemaOwner"] = result.schemaOwner;
    j["status"] = toString(result.status);
    j["summary"] = {
        {"relationshipsChecked", result.consistencyFindings.size()},
        {"relationshipsWithOrphans", violatingRelationships},
        {"totalOrphanRows", totalOrphans},
        {"dependencyCyclesFound", result.cycles.size()},
    };
    j["findings"] = result.consistencyFindings;
    j["dependencyCycles"] = result.cycles;
    writeJsonFile(j, outputDir, "consistency_report.json");
}

void writeAllReports(const core::ScanResult& result, const std::string& outputDir) {
    writeSchemaReport(result, outputDir);
    writeConsistencyReport(result, outputDir);
}

}  // namespace dbscanner::report
