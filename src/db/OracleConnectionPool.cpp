#include "db/OracleConnectionPool.hpp"

using namespace oracle::occi;

namespace dbscanner::db {

OracleConnectionPool::OracleConnectionPool(const std::string& connectString,
                                            const std::string& username, const std::string& password,
                                            int poolSize) {
    try {
        env_ = Environment::createEnvironment(Environment::THREADED_MUTEXED);
        allConnections_.reserve(static_cast<std::size_t>(poolSize));
        for (int i = 0; i < poolSize; ++i) {
            Connection* conn = env_->createConnection(username, password, connectString);
            allConnections_.push_back(conn);
            available_.push_back(conn);
        }
    } catch (const SQLException& e) {
        // Best-effort cleanup of whatever was already created before the failure.
        for (auto* conn : allConnections_) {
            if (env_) env_->terminateConnection(conn);
        }
        if (env_) Environment::terminateEnvironment(env_);
        throw OracleError("failed to establish Oracle connection pool: " + std::string(e.what()));
    }
}

OracleConnectionPool::~OracleConnectionPool() {
    for (auto* conn : allConnections_) {
        env_->terminateConnection(conn);
    }
    if (env_) Environment::terminateEnvironment(env_);
}

OracleConnectionPool::LeasedConnection OracleConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return !available_.empty(); });
    Connection* conn = available_.front();
    available_.pop_front();
    return LeasedConnection(this, conn);
}

void OracleConnectionPool::release(Connection* conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push_back(conn);
    }
    cv_.notify_one();
}

OracleConnectionPool::LeasedConnection::LeasedConnection(LeasedConnection&& other) noexcept
    : pool_(other.pool_), conn_(other.conn_) {
    other.pool_ = nullptr;
    other.conn_ = nullptr;
}

OracleConnectionPool::LeasedConnection& OracleConnectionPool::LeasedConnection::operator=(
    LeasedConnection&& other) noexcept {
    if (this != &other) {
        if (pool_ && conn_) pool_->release(conn_);
        pool_ = other.pool_;
        conn_ = other.conn_;
        other.pool_ = nullptr;
        other.conn_ = nullptr;
    }
    return *this;
}

OracleConnectionPool::LeasedConnection::~LeasedConnection() {
    if (pool_ && conn_) pool_->release(conn_);
}

}  // namespace dbscanner::db
