#pragma once

#include <string>
#include <vector>

#include "core/Relationship.hpp"
#include "core/TableMetadata.hpp"

namespace dbscanner::db {

// Abstraction over "something that can enumerate tables and describe their
// structure". The Oracle/OCCI implementation lives in OracleSchemaReader;
// this interface has no Oracle dependency so ScanEngine (and its tests) don't
// need a live database or the Instant Client SDK to build/run.
class ISchemaSource {
public:
    virtual ~ISchemaSource() = default;

    virtual std::vector<std::string> listTableNames() = 0;
    virtual core::TableInfo readTable(const std::string& tableName) = 0;

    // All declared (dictionary-level) foreign keys across the target schema,
    // as RelationshipKind::Declared relationships.
    virtual std::vector<core::Relationship> listDeclaredForeignKeys() = 0;
};

}  // namespace dbscanner::db
