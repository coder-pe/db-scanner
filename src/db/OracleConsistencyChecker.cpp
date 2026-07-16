#include "db/OracleConsistencyChecker.hpp"

#include <numeric>

#include "db/OracleStatementGuard.hpp"

using namespace oracle::occi;

namespace dbscanner::db {

namespace {

std::string quoteIdent(const std::string& name) {
    std::string escaped;
    escaped.reserve(name.size() + 2);
    escaped.push_back('"');
    for (char c : name) {
        if (c == '"') escaped.push_back('"');
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

std::string buildNotNullClause(const std::vector<std::string>& childColumns) {
    std::string clause;
    for (std::size_t i = 0; i < childColumns.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += "c." + quoteIdent(childColumns[i]) + " IS NOT NULL";
    }
    return clause;
}

std::string buildJoinClause(const std::vector<std::string>& parentColumns,
                             const std::vector<std::string>& childColumns) {
    std::string clause;
    for (std::size_t i = 0; i < parentColumns.size(); ++i) {
        if (i > 0) clause += " AND ";
        clause += "p." + quoteIdent(parentColumns[i]) + " = c." + quoteIdent(childColumns[i]);
    }
    return clause;
}

std::string buildAntiJoinWhere(const core::Relationship& rel) {
    return buildNotNullClause(rel.childColumns) + " AND NOT EXISTS (SELECT 1 FROM " +
           quoteIdent(rel.parentTable) + " p WHERE " + buildJoinClause(rel.parentColumns, rel.childColumns) +
           ")";
}

}  // namespace

OracleConsistencyChecker::OracleConsistencyChecker(OracleConnectionPool& pool) : pool_(pool) {}

core::ConsistencyFinding OracleConsistencyChecker::checkOrphans(const core::Relationship& relationship,
                                                                  int sampleLimit) {
    auto conn = pool_.acquire();
    Connection* c = conn.get();

    core::ConsistencyFinding finding;
    finding.relationship = relationship;

    const std::string whereClause = buildAntiJoinWhere(relationship);

    {
        const std::string countSql =
            "SELECT COUNT(*) FROM " + quoteIdent(relationship.childTable) + " c WHERE " + whereClause;
        detail::StatementGuard guard(c, countSql);
        ResultSet* rs = guard.executeQuery();
        if (rs->next()) {
            finding.orphanCount = static_cast<int64_t>(rs->getDouble(1));
        }
    }

    if (finding.orphanCount > 0 && sampleLimit > 0) {
        std::string selectCols;
        for (std::size_t i = 0; i < relationship.childColumns.size(); ++i) {
            if (i > 0) selectCols += ", ";
            selectCols += "c." + quoteIdent(relationship.childColumns[i]);
        }
        const std::string sampleSql = "SELECT " + selectCols + " FROM " +
                                       quoteIdent(relationship.childTable) + " c WHERE " + whereClause +
                                       " FETCH FIRST :1 ROWS ONLY";
        detail::StatementGuard guard(c, sampleSql);
        guard.stmt()->setInt(1, sampleLimit);
        ResultSet* rs = guard.executeQuery();
        while (rs->next()) {
            std::vector<std::string> row;
            row.reserve(relationship.childColumns.size());
            for (std::size_t i = 0; i < relationship.childColumns.size(); ++i) {
                row.push_back(rs->getString(static_cast<int>(i) + 1));
            }
            finding.sampleKeys.push_back(std::move(row));
        }
    }

    return finding;
}

}  // namespace dbscanner::db
