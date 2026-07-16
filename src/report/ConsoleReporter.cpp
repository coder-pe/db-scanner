#include "report/ConsoleReporter.hpp"

#include <cstdio>
#include <iostream>

#include <fmt/core.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace dbscanner::report {

bool isStdoutTty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

void printProgress(const std::string& phase, std::size_t current, std::size_t total,
                    const std::string& itemName) {
    const std::string line = fmt::format("[{}] {}/{} {}", phase, current, total, itemName);
    if (isStdoutTty()) {
        fmt::print("\r\x1b[2K{}", line);
        std::cout.flush();
    } else {
        fmt::print("{}\n", line);
    }
}

void endProgress() {
    if (isStdoutTty()) {
        fmt::print("\n");
        std::cout.flush();
    }
}

void printSummary(const core::ScanResult& result) {
    int64_t declared = 0;
    int64_t inferred = 0;
    for (const auto& rel : result.relationships) {
        if (rel.kind == core::RelationshipKind::Declared) {
            ++declared;
        } else {
            ++inferred;
        }
    }

    int64_t relationshipsWithOrphans = 0;
    int64_t totalOrphans = 0;
    for (const auto& finding : result.consistencyFindings) {
        if (finding.orphanCount > 0) {
            ++relationshipsWithOrphans;
            totalOrphans += finding.orphanCount;
        }
    }

    fmt::print("\n");
    fmt::print("==================== db-scanner summary ====================\n");
    fmt::print("Schema owner            : {}\n", result.schemaOwner);
    fmt::print("Status                  : {}\n", toString(result.status));
    fmt::print("Tables scanned          : {}\n", result.tables.size());
    fmt::print("Relationships (declared): {}\n", declared);
    fmt::print("Relationships (inferred): {}\n", inferred);
    fmt::print("Dependency cycles found : {}\n", result.cycles.size());
    fmt::print("Relationships checked   : {}\n", result.consistencyFindings.size());
    fmt::print("Relationships w/orphans : {}\n", relationshipsWithOrphans);
    fmt::print("Total orphan rows found : {}\n", totalOrphans);
    fmt::print("==============================================================\n");

    if (result.status == core::ScanStatus::Interrupted) {
        fmt::print("\nRun was interrupted. Re-run with --resume to continue where it left off.\n");
    }
}

}  // namespace dbscanner::report
