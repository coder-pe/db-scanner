#pragma once

#include "db/IConsistencyChecker.hpp"
#include "db/OracleConnectionPool.hpp"

namespace dbscanner::db {

// Checks referential consistency for a single relationship by running an
// anti-join (NOT EXISTS) query: child rows whose FK column(s) are all
// non-NULL (matching Oracle's default MATCH SIMPLE semantics for composite
// keys) but have no matching parent row.
class OracleConsistencyChecker : public IConsistencyChecker {
public:
    explicit OracleConsistencyChecker(OracleConnectionPool& pool);

    core::ConsistencyFinding checkOrphans(const core::Relationship& relationship,
                                           int sampleLimit) override;

private:
    OracleConnectionPool& pool_;
};

}  // namespace dbscanner::db
