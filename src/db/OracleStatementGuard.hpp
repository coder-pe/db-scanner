#pragma once

#include <string>

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

}  // namespace dbscanner::db::detail
