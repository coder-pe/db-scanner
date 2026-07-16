#pragma once

#include <cstddef>
#include <string>

#include "core/ScanResult.hpp"

namespace dbscanner::report {

bool isStdoutTty();

// Prints a single-line progress update (overwriting the previous one when
// stdout is a TTY, otherwise appending a new line so redirected/log output
// stays readable).
void printProgress(const std::string& phase, std::size_t current, std::size_t total,
                    const std::string& itemName);

// Clears the in-place progress line (call once a phase finishes, before
// printing anything else), only meaningful when stdout is a TTY.
void endProgress();

void printSummary(const core::ScanResult& result);

}  // namespace dbscanner::report
