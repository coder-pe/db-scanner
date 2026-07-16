#include "checkpoint/CheckpointStore.hpp"

#include <sqlite3.h>

#include "core/TimeUtil.hpp"

namespace dbscanner::checkpoint {

namespace {

using core::nowIso;

// RAII wrapper around a prepared statement, always finalized on scope exit.
class Stmt {
public:
    Stmt(sqlite3* db, const std::string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw CheckpointError(std::string("failed to prepare statement: ") + sqlite3_errmsg(db));
        }
    }
    ~Stmt() { sqlite3_finalize(stmt_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void execOrThrow(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string message = errMsg ? errMsg : "unknown sqlite error";
        sqlite3_free(errMsg);
        throw CheckpointError("sqlite exec failed: " + message);
    }
}

}  // namespace

std::string toString(UnitStatus status) {
    switch (status) {
        case UnitStatus::Pending:
            return "pending";
        case UnitStatus::InProgress:
            return "in_progress";
        case UnitStatus::Done:
            return "done";
        case UnitStatus::Error:
            return "error";
    }
    return "pending";
}

UnitStatus unitStatusFromString(const std::string& text) {
    if (text == "in_progress") return UnitStatus::InProgress;
    if (text == "done") return UnitStatus::Done;
    if (text == "error") return UnitStatus::Error;
    return UnitStatus::Pending;
}

CheckpointStore::CheckpointStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::string message = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw CheckpointError("failed to open checkpoint db '" + dbPath + "': " + message);
    }

    execOrThrow(db_, "PRAGMA journal_mode=WAL;");
    execOrThrow(db_, R"(
        CREATE TABLE IF NOT EXISTS run_info (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
    execOrThrow(db_, R"(
        CREATE TABLE IF NOT EXISTS table_status (
            table_name TEXT PRIMARY KEY,
            status TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            error_message TEXT,
            payload TEXT
        );
    )");
    execOrThrow(db_, R"(
        CREATE TABLE IF NOT EXISTS relation_status (
            relation_id TEXT PRIMARY KEY,
            status TEXT NOT NULL,
            updated_at TEXT NOT NULL,
            error_message TEXT,
            payload TEXT
        );
    )");
}

CheckpointStore::~CheckpointStore() {
    if (db_) sqlite3_close(db_);
}

bool CheckpointStore::isFreshRun() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt stmt(db_, "SELECT COUNT(*) FROM run_info;");
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        throw CheckpointError("failed to query run_info");
    }
    return sqlite3_column_int(stmt.get(), 0) == 0;
}

void CheckpointStore::setRunInfo(const std::map<std::string, std::string>& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    execOrThrow(db_, "BEGIN;");
    try {
        Stmt stmt(db_, "INSERT INTO run_info(key, value) VALUES (?, ?) "
                       "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
        for (const auto& [key, value] : info) {
            sqlite3_reset(stmt.get());
            sqlite3_clear_bindings(stmt.get());
            bindText(stmt.get(), 1, key);
            bindText(stmt.get(), 2, value);
            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                throw CheckpointError(std::string("failed to write run_info: ") + sqlite3_errmsg(db_));
            }
        }
        execOrThrow(db_, "COMMIT;");
    } catch (...) {
        execOrThrow(db_, "ROLLBACK;");
        throw;
    }
}

std::map<std::string, std::string> CheckpointStore::getRunInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, std::string> result;
    Stmt stmt(db_, "SELECT key, value FROM run_info;");
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const auto* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        result[key ? key : ""] = value ? value : "";
    }
    return result;
}

namespace {
void upsertStatus(sqlite3* db, std::mutex& mutex, const std::string& table, const std::string& idColumn,
                   const std::string& id, UnitStatus status, const std::string& errorMessage,
                   const std::optional<std::string>& payload) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::string sql =
        "INSERT INTO " + table + "(" + idColumn +
        ", status, updated_at, error_message, payload) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(" +
        idColumn +
        ") DO UPDATE SET status = excluded.status, updated_at = excluded.updated_at, "
        "error_message = excluded.error_message, payload = COALESCE(excluded.payload, " +
        table + ".payload);";
    Stmt stmt(db, sql);
    bindText(stmt.get(), 1, id);
    bindText(stmt.get(), 2, toString(status));
    bindText(stmt.get(), 3, nowIso());
    bindText(stmt.get(), 4, errorMessage);
    if (payload.has_value()) {
        bindText(stmt.get(), 5, *payload);
    } else {
        sqlite3_bind_null(stmt.get(), 5);
    }
    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        throw CheckpointError(std::string("failed to write status: ") + sqlite3_errmsg(db));
    }
}

UnitStatus getStatus(sqlite3* db, std::mutex& mutex, const std::string& table, const std::string& idColumn,
                      const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::string sql = "SELECT status FROM " + table + " WHERE " + idColumn + " = ?;";
    Stmt stmt(db, sql);
    bindText(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return UnitStatus::Pending;
    }
    const auto* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
    return unitStatusFromString(status ? status : "");
}

std::optional<std::string> getPayload(sqlite3* db, std::mutex& mutex, const std::string& table,
                                       const std::string& idColumn, const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::string sql = "SELECT payload FROM " + table + " WHERE " + idColumn + " = ?;";
    Stmt stmt(db, sql);
    bindText(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) != SQLITE_ROW || sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
        return std::nullopt;
    }
    const auto* payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
    return payload ? std::optional<std::string>(payload) : std::nullopt;
}

std::vector<std::string> idsWithStatus(sqlite3* db, std::mutex& mutex, const std::string& table,
                                        const std::string& idColumn, UnitStatus status) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::string sql = "SELECT " + idColumn + " FROM " + table + " WHERE status = ?;";
    Stmt stmt(db, sql);
    bindText(stmt.get(), 1, toString(status));
    std::vector<std::string> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const auto* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        if (id) result.emplace_back(id);
    }
    return result;
}
}  // namespace

void CheckpointStore::markTableStatus(const std::string& tableName, UnitStatus status,
                                       const std::string& errorMessage) {
    upsertStatus(db_, mutex_, "table_status", "table_name", tableName, status, errorMessage,
                 std::nullopt);
}

void CheckpointStore::markTableDone(const std::string& tableName, const std::string& payloadJson) {
    upsertStatus(db_, mutex_, "table_status", "table_name", tableName, UnitStatus::Done, "",
                 payloadJson);
}

UnitStatus CheckpointStore::getTableStatus(const std::string& tableName) const {
    return getStatus(db_, mutex_, "table_status", "table_name", tableName);
}

std::vector<std::string> CheckpointStore::tableNamesWithStatus(UnitStatus status) const {
    return idsWithStatus(db_, mutex_, "table_status", "table_name", status);
}

std::optional<std::string> CheckpointStore::getTablePayload(const std::string& tableName) const {
    return getPayload(db_, mutex_, "table_status", "table_name", tableName);
}

void CheckpointStore::markRelationStatus(const std::string& relationId, UnitStatus status,
                                          const std::string& errorMessage) {
    upsertStatus(db_, mutex_, "relation_status", "relation_id", relationId, status, errorMessage,
                 std::nullopt);
}

void CheckpointStore::markRelationDone(const std::string& relationId, const std::string& payloadJson) {
    upsertStatus(db_, mutex_, "relation_status", "relation_id", relationId, UnitStatus::Done, "",
                 payloadJson);
}

UnitStatus CheckpointStore::getRelationStatus(const std::string& relationId) const {
    return getStatus(db_, mutex_, "relation_status", "relation_id", relationId);
}

std::vector<std::string> CheckpointStore::relationIdsWithStatus(UnitStatus status) const {
    return idsWithStatus(db_, mutex_, "relation_status", "relation_id", status);
}

std::optional<std::string> CheckpointStore::getRelationPayload(const std::string& relationId) const {
    return getPayload(db_, mutex_, "relation_status", "relation_id", relationId);
}

void CheckpointStore::resetInProgressToPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    execOrThrow(db_, "UPDATE table_status SET status = 'pending' WHERE status = 'in_progress';");
    execOrThrow(db_, "UPDATE relation_status SET status = 'pending' WHERE status = 'in_progress';");
}

}  // namespace dbscanner::checkpoint
