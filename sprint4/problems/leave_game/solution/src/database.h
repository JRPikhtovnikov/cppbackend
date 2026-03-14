#pragma once

#include <pqxx/pqxx>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <chrono>

namespace db {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    template <typename Factory>
    ConnectionPool(size_t capacity, Factory&& factory) {
        pool_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(factory());
        }
    }

    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept
            : conn_(std::move(conn)), pool_(&pool) {}

        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        ConnectionWrapper(ConnectionWrapper&&) = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

        pqxx::connection& operator*() const noexcept { return *conn_; }
        pqxx::connection* operator->() const noexcept { return conn_.get(); }

        ~ConnectionWrapper() {
            if (conn_) pool_->ReturnConnection(std::move(conn_));
        }

    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };

    ConnectionWrapper GetConnection() {
        std::unique_lock lock(mutex_);
        cond_var_.wait(lock, [this] { return used_ < pool_.size(); });
        return ConnectionWrapper(std::move(pool_[used_++]), *this);
    }

private:
    void ReturnConnection(ConnectionPtr&& conn) {
        {
            std::lock_guard lock(mutex_);
            assert(used_ > 0);
            pool_[--used_] = std::move(conn);
        }
        cond_var_.notify_one();
    }

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    size_t used_ = 0;
};

struct RetiredPlayerRecord {
    std::string name;
    int score;
    std::chrono::milliseconds play_time;
};

class Database {
public:
    explicit Database(ConnectionPool& pool) : pool_(pool) {}

    void Initialize();

    void InsertRetiredPlayer(const RetiredPlayerRecord& record);

    std::vector<RetiredPlayerRecord> GetRecords(size_t start, size_t max_items);

private:
    ConnectionPool& pool_;
};

} // namespace db