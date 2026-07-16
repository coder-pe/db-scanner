#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dbscanner::engine {

// Simple fixed-size worker pool. submit() returns a future so callers can
// batch a phase's work and block on all of it as a barrier before starting
// the next phase (see ScanEngine).
class ThreadPool {
public:
    explicit ThreadPool(int numThreads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Fn>
    auto submit(Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
        using ReturnType = std::invoke_result_t<Fn>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<Fn>(fn));
        std::future<ReturnType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}  // namespace dbscanner::engine
