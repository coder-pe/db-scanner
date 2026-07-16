#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include <occi.h>

namespace dbscanner::db::detail {

// RAII wrapper ensuring an OCCI Statement (and any ResultSet opened from it)
// is always closed/terminated, even if an exception is thrown while iterating.
class StatementGuard {
public:
    StatementGuard(oracle::occi::Connection* conn, const std::string& sql) : conn_(conn) {
        stmt_ = conn_->createStatement(sql);
    }

    ~StatementGuard() {
        if (stmt_) {
            if (rs_) stmt_->closeResultSet(rs_);
            conn_->terminateStatement(stmt_);
        }
    }

    StatementGuard(const StatementGuard&) = delete;
    StatementGuard& operator=(const StatementGuard&) = delete;

    oracle::occi::Statement* stmt() const { return stmt_; }

    oracle::occi::ResultSet* executeQuery() {
        rs_ = stmt_->executeQuery();
        return rs_;
    }

private:
    oracle::occi::Connection* conn_;
    oracle::occi::Statement* stmt_ = nullptr;
    oracle::occi::ResultSet* rs_ = nullptr;
};

// Runs fn(), and on any failure rethrows a std::runtime_error prefixed with
// step (e.g. "fetchPrimaryKey", "listDeclaredForeignKeys") so error logs say
// which specific data-dictionary query failed, not just which table -- OCCI's
// oracle::occi::SQLException::what() alone gives no clue which of several
// queries against a table raised it.
template <typename Fn>
auto withStep(const char* step, Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const std::exception& e) {
        // Covers oracle::occi::SQLException too (it derives from std::exception).
        throw std::runtime_error(std::string("[") + step + "] " + e.what());
    }
}

}  // namespace dbscanner::db::detail
