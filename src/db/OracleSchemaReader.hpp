#pragma once

#include <string>

#include "db/ISchemaSource.hpp"
#include "db/OracleConnectionPool.hpp"

namespace dbscanner::db {

// Reads table structure (columns, keys, indexes, comments, approx row count)
// and declared foreign keys from the Oracle data dictionary (ALL_TABLES,
// ALL_TAB_COLUMNS, ALL_CONSTRAINTS, ALL_CONS_COLUMNS, ALL_INDEXES,
// ALL_IND_COLUMNS, ALL_TAB_COMMENTS, ALL_COL_COMMENTS) for a single owner.
class OracleSchemaReader : public ISchemaSource {
public:
    OracleSchemaReader(OracleConnectionPool& pool, std::string schemaOwner);

    std::vector<std::string> listTableNames() override;
    core::TableInfo readTable(const std::string& tableName) override;
    std::vector<core::Relationship> listDeclaredForeignKeys() override;

private:
    OracleConnectionPool& pool_;
    std::string schemaOwner_;
};

}  // namespace dbscanner::db
