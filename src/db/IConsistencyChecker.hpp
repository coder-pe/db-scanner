#pragma once

#include "core/Relationship.hpp"
#include "core/ScanResult.hpp"

namespace dbscanner::db {

// Abstraction over "something that can check a single relationship for
// referential-consistency violations (orphaned child rows)". See
// ISchemaSource.hpp for why this is split out as an interface.
class IConsistencyChecker {
public:
    virtual ~IConsistencyChecker() = default;

    virtual core::ConsistencyFinding checkOrphans(const core::Relationship& relationship,
                                                   int sampleLimit) = 0;
};

}  // namespace dbscanner::db
