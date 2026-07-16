#pragma once

#include <string>

#include "core/ScanResult.hpp"

namespace dbscanner::report {

// Writes <outputDir>/schema.json (tables, columns, relationships, dependency
// cycles) and <outputDir>/consistency_report.json (referential-consistency
// findings). Creates outputDir if it doesn't exist. Throws std::runtime_error
// on I/O failure.
void writeSchemaReport(const core::ScanResult& result, const std::string& outputDir);
void writeConsistencyReport(const core::ScanResult& result, const std::string& outputDir);
void writeAllReports(const core::ScanResult& result, const std::string& outputDir);

}  // namespace dbscanner::report
