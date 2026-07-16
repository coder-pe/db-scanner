#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;

namespace dbscanner::checkpoint {

struct CheckpointError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class UnitStatus { Pending, InProgress, Done, Error };

std::string toString(UnitStatus status);
UnitStatus unitStatusFromString(const std::string& text);

// Thread-safe SQLite-backed store recording scan progress so an interrupted
// run can resume without redoing already-completed work. One store instance
// maps to one <output_dir>/checkpoint.db file, representing a single scan
// run's progress.
class CheckpointStore {
public:
    // Opens (creating if necessary) the SQLite checkpoint database at dbPath
    // and ensures the schema exists. Throws CheckpointError on failure.
    explicit CheckpointStore(const std::string& dbPath);

    CheckpointStore(CheckpointStore&&) = delete;
    CheckpointStore& operator=(CheckpointStore&&) = delete;
    CheckpointStore(const CheckpointStore&) = delete;
    CheckpointStore& operator=(const CheckpointStore&) = delete;
    ~CheckpointStore();

    // True if this store had no prior run_info (i.e. it's a brand-new run, not
    // one being resumed).
    bool isFreshRun() const;

    void setRunInfo(const std::map<std::string, std::string>& info);
    std::map<std::string, std::string> getRunInfo() const;

    void markTableStatus(const std::string& tableName, UnitStatus status,
                          const std::string& errorMessage = "");
    // Marks a table Done and stores its result payload (e.g. TableInfo
    // serialized to JSON) atomically, so a resumed run can load the result
    // without re-querying Oracle for tables already completed.
    void markTableDone(const std::string& tableName, const std::string& payloadJson);
    UnitStatus getTableStatus(const std::string& tableName) const;
    std::vector<std::string> tableNamesWithStatus(UnitStatus status) const;
    std::optional<std::string> getTablePayload(const std::string& tableName) const;

    void markRelationStatus(const std::string& relationId, UnitStatus status,
                             const std::string& errorMessage = "");
    void markRelationDone(const std::string& relationId, const std::string& payloadJson);
    UnitStatus getRelationStatus(const std::string& relationId) const;
    std::vector<std::string> relationIdsWithStatus(UnitStatus status) const;
    std::optional<std::string> getRelationPayload(const std::string& relationId) const;

    // Resets any unit left `InProgress` (e.g. from a crash without clean
    // shutdown) back to `Pending` so it gets retried on resume. Call once
    // right after opening a store you intend to resume from.
    void resetInProgressToPending();

private:
    sqlite3* db_;
    mutable std::mutex mutex_;
};

}  // namespace dbscanner::checkpoint
