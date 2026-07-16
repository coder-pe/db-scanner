#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <occi.h>

namespace dbscanner::db {

struct OracleError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Owns an OCCI Environment and a fixed-size pool of Connections, sized to the
// scan's worker thread count so each worker gets exclusive use of a
// connection for the duration of a unit of work (one table / one relationship
// check). Blocks acquire() if every connection is currently leased.
class OracleConnectionPool {
public:
    OracleConnectionPool(const std::string& connectString, const std::string& username,
                          const std::string& password, int poolSize);
    ~OracleConnectionPool();

    OracleConnectionPool(const OracleConnectionPool&) = delete;
    OracleConnectionPool& operator=(const OracleConnectionPool&) = delete;

    class LeasedConnection {
    public:
        ~LeasedConnection();
        LeasedConnection(LeasedConnection&& other) noexcept;
        LeasedConnection& operator=(LeasedConnection&& other) noexcept;
        LeasedConnection(const LeasedConnection&) = delete;
        LeasedConnection& operator=(const LeasedConnection&) = delete;

        oracle::occi::Connection* operator->() const { return conn_; }
        oracle::occi::Connection* get() const { return conn_; }

    private:
        friend class OracleConnectionPool;
        LeasedConnection(OracleConnectionPool* pool, oracle::occi::Connection* conn)
            : pool_(pool), conn_(conn) {}

        OracleConnectionPool* pool_;
        oracle::occi::Connection* conn_;
    };

    LeasedConnection acquire();

private:
    void release(oracle::occi::Connection* conn);

    oracle::occi::Environment* env_ = nullptr;
    std::vector<oracle::occi::Connection*> allConnections_;
    std::deque<oracle::occi::Connection*> available_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace dbscanner::db
