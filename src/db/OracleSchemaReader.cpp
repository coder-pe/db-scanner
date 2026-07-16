#include "db/OracleSchemaReader.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <optional>

#include <spdlog/spdlog.h>

#include "db/OracleStatementGuard.hpp"

using namespace oracle::occi;

namespace dbscanner::db {

namespace {

// NOTE: none of the queries below use ORDER BY against ALL_TAB_COLUMNS,
// ALL_CONSTRAINTS or ALL_INDEXES, and ordering/grouping is done client-side
// in C++ instead. This turned out not to be the actual cause of the
// ORA-00932 errors seen in practice (that was DATA_DEFAULT, a LONG column,
// being referenced in a WHERE clause -- see fetchDefaultValues below, which
// Oracle flatly disallows for any LONG column). It's kept anyway as a
// low-risk simplification: these queries are cheap, per-table, and this
// avoids relying on ORDER BY against views known to embed a LONG column
// internally.

std::vector<core::Column> fetchColumns(Connection* conn, const std::string& owner,
                                        const std::string& table) {
    detail::StatementGuard guard(conn, R"(
        SELECT COLUMN_NAME, DATA_TYPE, DATA_LENGTH, DATA_PRECISION, DATA_SCALE,
               NULLABLE, COLUMN_ID
        FROM ALL_TAB_COLUMNS
        WHERE OWNER = :1 AND TABLE_NAME = :2
    )");
    guard.stmt()->setString(1, owner);
    guard.stmt()->setString(2, table);
    ResultSet* rs = guard.executeQuery();

    std::vector<core::Column> columns;
    while (rs->next()) {
        core::Column col;
        col.name = rs->getString(1);
        col.dataType = rs->getString(2);
        col.dataLength = rs->getInt(3);
        if (!rs->isNull(4)) col.dataPrecision = rs->getInt(4);
        if (!rs->isNull(5)) col.dataScale = rs->getInt(5);
        col.nullable = (rs->getString(6) == "Y");
        col.columnId = rs->getInt(7);
        columns.push_back(std::move(col));
    }
    std::sort(columns.begin(), columns.end(),
              [](const core::Column& a, const core::Column& b) { return a.columnId < b.columnId; });
    return columns;
}

// Four prior attempts at reading ALL_TAB_COLUMNS.DATA_DEFAULT directly all
// failed the same way (ORA-00932 / ORA-32108), with or without SUBSTR(), with
// or without bind variables. Confirmed via sqlplus that the plain query
// against DATA_DEFAULT works fine outside OCCI, isolating this to how OCCI
// itself handles a LONG column (most likely: LONG is incompatible with
// OCCI's default row prefetching). Rather than keep fighting LONG's client
// library restrictions, default values are read from the table's DDL text
// instead -- DBMS_METADATA.GET_DDL returns a CLOB, which has none of LONG's
// restrictions (binds, WHERE clauses, prefetch all work normally on it).
//
// The DDL is parsed with a small heuristic text scanner, not a real SQL
// parser: for each already-known column name (from fetchColumns), find its
// quoted declaration in the DDL, take the text up to that column
// definition's closing comma/paren (paren-depth aware, so DEFAULT expressions
// containing commas aren't cut short), and pull out anything after a
// "DEFAULT" keyword. This is best-effort -- parsing failures just leave that
// column's defaultValue unset, they never fail the table scan.
std::string readClobAsString(Clob& clob) {
    clob.open(OCCI_LOB_READONLY);
    std::string content;
    const int length = clob.length();
    if (length > 0) {
        content.resize(static_cast<std::size_t>(length));
        Stream* stream = clob.getStream();
        stream->readBuffer(&content[0], length);
        clob.closeStream(stream);
    }
    clob.close();
    return content;
}

std::optional<std::string> extractColumnDefault(const std::string& ddl, const std::string& columnName) {
    const std::string needle = "\"" + columnName + "\"";
    const std::size_t nameEnd = ddl.find(needle);
    if (nameEnd == std::string::npos) return std::nullopt;

    std::size_t i = nameEnd + needle.size();
    int depth = 0;
    for (; i < ddl.size(); ++i) {
        const char c = ddl[i];
        if (c == '(') {
            ++depth;
        } else if (c == ')') {
            if (depth == 0) break;  // closing paren of the column list itself
            --depth;
        } else if (c == ',' && depth == 0) {
            break;
        }
    }
    const std::string columnDef = ddl.substr(nameEnd + needle.size(), i - (nameEnd + needle.size()));

    // Find " DEFAULT " as a whole word, case-insensitively.
    auto toUpper = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
        return s;
    };
    const std::string upperDef = toUpper(columnDef);
    const std::size_t defaultPos = upperDef.find(" DEFAULT ");
    if (defaultPos == std::string::npos) return std::nullopt;

    std::string value = columnDef.substr(defaultPos + std::string(" DEFAULT ").size());

    // Strip a trailing " NOT NULL" / " NULL" column-constraint if present.
    const std::string upperValue = toUpper(value);
    for (const char* suffix : {" NOT NULL", " NULL"}) {
        const std::size_t suffixLen = std::strlen(suffix);
        const std::size_t pos = upperValue.rfind(suffix);
        if (pos != std::string::npos && pos + suffixLen == upperValue.size()) {
            value = value.substr(0, pos);
            break;
        }
    }

    // Trim whitespace.
    const std::size_t begin = value.find_first_not_of(" \t\n\r");
    if (begin == std::string::npos) return std::nullopt;
    const std::size_t end = value.find_last_not_of(" \t\n\r");
    value = value.substr(begin, end - begin + 1);

    if (value.empty()) return std::nullopt;
    if (value.size() > 4000) value.resize(4000);
    return value;
}

void fetchDefaultValues(Connection* conn, const std::string& owner, const std::string& table,
                         std::vector<core::Column>& columns) {
    detail::StatementGuard guard(conn, "SELECT DBMS_METADATA.GET_DDL('TABLE', :1, :2) FROM DUAL");
    guard.stmt()->setString(1, table);
    guard.stmt()->setString(2, owner);
    ResultSet* rs = guard.executeQuery();
    if (!rs->next()) return;

    Clob clob = rs->getClob(1);
    const std::string ddl = readClobAsString(clob);

    for (auto& col : columns) {
        if (auto def = extractColumnDefault(ddl, col.name)) {
            col.defaultValue = *def;
        }
    }
}

void fetchComments(Connection* conn, const std::string& owner, const std::string& table,
                    core::TableInfo& info) {
    {
        detail::StatementGuard guard(conn,
                                      "SELECT COMMENTS FROM ALL_TAB_COMMENTS "
                                      "WHERE OWNER = :1 AND TABLE_NAME = :2 AND COMMENTS IS NOT NULL");
        guard.stmt()->setString(1, owner);
        guard.stmt()->setString(2, table);
        ResultSet* rs = guard.executeQuery();
        if (rs->next()) {
            info.comment = rs->getString(1);
        }
    }
    {
        detail::StatementGuard guard(conn,
                                      "SELECT COLUMN_NAME, COMMENTS FROM ALL_COL_COMMENTS "
                                      "WHERE OWNER = :1 AND TABLE_NAME = :2 AND COMMENTS IS NOT NULL");
        guard.stmt()->setString(1, owner);
        guard.stmt()->setString(2, table);
        ResultSet* rs = guard.executeQuery();
        while (rs->next()) {
            const std::string columnName = rs->getString(1);
            const std::string comment = rs->getString(2);
            for (auto& col : info.columns) {
                if (col.name == columnName) {
                    col.comment = comment;
                    break;
                }
            }
        }
    }
}

std::optional<core::PrimaryKey> fetchPrimaryKey(Connection* conn, const std::string& owner,
                                                 const std::string& table) {
    detail::StatementGuard guard(conn, R"(
        SELECT ac.CONSTRAINT_NAME, acc.COLUMN_NAME, acc.POSITION
        FROM ALL_CONSTRAINTS ac
        JOIN ALL_CONS_COLUMNS acc
          ON ac.OWNER = acc.OWNER AND ac.CONSTRAINT_NAME = acc.CONSTRAINT_NAME
        WHERE ac.OWNER = :1 AND ac.TABLE_NAME = :2 AND ac.CONSTRAINT_TYPE = 'P'
    )");
    guard.stmt()->setString(1, owner);
    guard.stmt()->setString(2, table);
    ResultSet* rs = guard.executeQuery();

    std::string constraintName;
    std::vector<std::pair<int, std::string>> positioned;
    while (rs->next()) {
        constraintName = rs->getString(1);
        positioned.emplace_back(rs->getInt(3), rs->getString(2));
    }
    if (positioned.empty()) return std::nullopt;

    std::sort(positioned.begin(), positioned.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    core::PrimaryKey pk;
    pk.constraintName = constraintName;
    for (const auto& [position, columnName] : positioned) pk.columns.push_back(columnName);
    return pk;
}

std::vector<core::UniqueKey> fetchUniqueKeys(Connection* conn, const std::string& owner,
                                              const std::string& table) {
    detail::StatementGuard guard(conn, R"(
        SELECT ac.CONSTRAINT_NAME, acc.COLUMN_NAME, acc.POSITION
        FROM ALL_CONSTRAINTS ac
        JOIN ALL_CONS_COLUMNS acc
          ON ac.OWNER = acc.OWNER AND ac.CONSTRAINT_NAME = acc.CONSTRAINT_NAME
        WHERE ac.OWNER = :1 AND ac.TABLE_NAME = :2 AND ac.CONSTRAINT_TYPE = 'U'
    )");
    guard.stmt()->setString(1, owner);
    guard.stmt()->setString(2, table);
    ResultSet* rs = guard.executeQuery();

    std::map<std::string, std::vector<std::pair<int, std::string>>> byConstraint;
    while (rs->next()) {
        const std::string name = rs->getString(1);
        byConstraint[name].emplace_back(rs->getInt(3), rs->getString(2));
    }

    std::vector<core::UniqueKey> keys;
    for (auto& [name, positioned] : byConstraint) {
        std::sort(positioned.begin(), positioned.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        core::UniqueKey key;
        key.constraintName = name;
        for (const auto& [position, columnName] : positioned) key.columns.push_back(columnName);
        keys.push_back(std::move(key));
    }
    return keys;
}

std::vector<core::IndexInfo> fetchIndexes(Connection* conn, const std::string& owner,
                                           const std::string& table) {
    detail::StatementGuard guard(conn, R"(
        SELECT ai.INDEX_NAME, ai.UNIQUENESS, aic.COLUMN_NAME, aic.COLUMN_POSITION
        FROM ALL_INDEXES ai
        JOIN ALL_IND_COLUMNS aic
          ON ai.OWNER = aic.INDEX_OWNER AND ai.INDEX_NAME = aic.INDEX_NAME
        WHERE ai.OWNER = :1 AND ai.TABLE_NAME = :2
    )");
    guard.stmt()->setString(1, owner);
    guard.stmt()->setString(2, table);
    ResultSet* rs = guard.executeQuery();

    std::map<std::string, std::vector<std::pair<int, std::string>>> columnsByIndex;
    std::map<std::string, bool> uniquenessByIndex;
    while (rs->next()) {
        const std::string name = rs->getString(1);
        uniquenessByIndex[name] = (rs->getString(2) == "UNIQUE");
        columnsByIndex[name].emplace_back(rs->getInt(4), rs->getString(3));
    }

    std::vector<core::IndexInfo> indexes;
    for (auto& [name, positioned] : columnsByIndex) {
        std::sort(positioned.begin(), positioned.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        core::IndexInfo info;
        info.name = name;
        info.isUnique = uniquenessByIndex[name];
        for (const auto& [position, columnName] : positioned) info.columns.push_back(columnName);
        indexes.push_back(std::move(info));
    }
    return indexes;
}

std::optional<int64_t> fetchApproxRowCount(Connection* conn, const std::string& owner,
                                            const std::string& table) {
    detail::StatementGuard guard(conn,
                                  "SELECT NUM_ROWS FROM ALL_TABLES WHERE OWNER = :1 AND TABLE_NAME = :2");
    guard.stmt()->setString(1, owner);
    guard.stmt()->setString(2, table);
    ResultSet* rs = guard.executeQuery();
    if (rs->next() && !rs->isNull(1)) {
        return static_cast<int64_t>(rs->getDouble(1));
    }
    return std::nullopt;
}

}  // namespace

OracleSchemaReader::OracleSchemaReader(OracleConnectionPool& pool, std::string schemaOwner)
    : pool_(pool), schemaOwner_(std::move(schemaOwner)) {}

std::vector<std::string> OracleSchemaReader::listTableNames() {
    return detail::withStep("listTableNames", [&] {
        auto conn = pool_.acquire();
        // ALL_TABLES has no LONG column, so ORDER BY here is safe (and this
        // query already succeeds in practice, unlike the ones against
        // ALL_TAB_COLUMNS/ALL_CONSTRAINTS/ALL_INDEXES above).
        detail::StatementGuard guard(
            conn.get(), "SELECT TABLE_NAME FROM ALL_TABLES WHERE OWNER = :1 ORDER BY TABLE_NAME");
        guard.stmt()->setString(1, schemaOwner_);
        ResultSet* rs = guard.executeQuery();

        std::vector<std::string> names;
        while (rs->next()) {
            names.push_back(rs->getString(1));
        }
        return names;
    });
}

core::TableInfo OracleSchemaReader::readTable(const std::string& tableName) {
    auto conn = pool_.acquire();
    Connection* c = conn.get();

    core::TableInfo info;
    info.owner = schemaOwner_;
    info.name = tableName;
    info.columns =
        detail::withStep("fetchColumns", [&] { return fetchColumns(c, schemaOwner_, tableName); });
    try {
        detail::withStep("fetchDefaultValues",
                          [&] { fetchDefaultValues(c, schemaOwner_, tableName, info.columns); });
    } catch (const std::exception& e) {
        // Column default values are cosmetic metadata, not core to structure/
        // relationship/consistency scanning -- don't let a LONG-column quirk
        // on this one query abort discovery for the whole table.
        spdlog::warn("could not fetch default values for table {}: {}", tableName, e.what());
    }
    detail::withStep("fetchComments", [&] { fetchComments(c, schemaOwner_, tableName, info); });
    info.primaryKey =
        detail::withStep("fetchPrimaryKey", [&] { return fetchPrimaryKey(c, schemaOwner_, tableName); });
    info.uniqueKeys =
        detail::withStep("fetchUniqueKeys", [&] { return fetchUniqueKeys(c, schemaOwner_, tableName); });
    info.indexes =
        detail::withStep("fetchIndexes", [&] { return fetchIndexes(c, schemaOwner_, tableName); });
    info.approxRowCount = detail::withStep(
        "fetchApproxRowCount", [&] { return fetchApproxRowCount(c, schemaOwner_, tableName); });
    return info;
}

std::vector<core::Relationship> OracleSchemaReader::listDeclaredForeignKeys() {
    return detail::withStep("listDeclaredForeignKeys", [&] { return listDeclaredForeignKeysImpl(); });
}

std::vector<core::Relationship> OracleSchemaReader::listDeclaredForeignKeysImpl() {
    auto conn = pool_.acquire();
    detail::StatementGuard guard(conn.get(), R"(
        SELECT ac.CONSTRAINT_NAME, ac.TABLE_NAME AS CHILD_TABLE, acc.COLUMN_NAME AS CHILD_COLUMN,
               rc.TABLE_NAME AS PARENT_TABLE, rcc.COLUMN_NAME AS PARENT_COLUMN, acc.POSITION
        FROM ALL_CONSTRAINTS ac
        JOIN ALL_CONS_COLUMNS acc
          ON ac.OWNER = acc.OWNER AND ac.CONSTRAINT_NAME = acc.CONSTRAINT_NAME
        JOIN ALL_CONSTRAINTS rc
          ON ac.R_OWNER = rc.OWNER AND ac.R_CONSTRAINT_NAME = rc.CONSTRAINT_NAME
        JOIN ALL_CONS_COLUMNS rcc
          ON rc.OWNER = rcc.OWNER AND rc.CONSTRAINT_NAME = rcc.CONSTRAINT_NAME
         AND rcc.POSITION = acc.POSITION
        WHERE ac.OWNER = :1 AND ac.CONSTRAINT_TYPE = 'R'
    )");
    guard.stmt()->setString(1, schemaOwner_);
    ResultSet* rs = guard.executeQuery();

    struct Row {
        std::string childTable;
        std::string childColumn;
        std::string parentTable;
        std::string parentColumn;
        int position;
    };
    std::map<std::string, std::vector<Row>> byConstraint;
    while (rs->next()) {
        const std::string constraintName = rs->getString(1);
        byConstraint[constraintName].push_back(Row{rs->getString(2), rs->getString(3), rs->getString(4),
                                                     rs->getString(5), rs->getInt(6)});
    }

    std::vector<core::Relationship> relationships;
    for (auto& [constraintName, rows] : byConstraint) {
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.position < b.position; });

        core::Relationship rel;
        rel.constraintName = constraintName;
        rel.childTable = rows.front().childTable;
        rel.parentTable = rows.front().parentTable;
        rel.kind = core::RelationshipKind::Declared;
        rel.confidence = 1.0;
        for (const auto& row : rows) {
            rel.childColumns.push_back(row.childColumn);
            rel.parentColumns.push_back(row.parentColumn);
        }
        relationships.push_back(std::move(rel));
    }
    return relationships;
}

}  // namespace dbscanner::db
