#pragma once

#include <chrono>
#include <exception>
#include <random>
#include <thread>
#include <utility>

namespace dbscanner::db {

struct RetryOptions {
    int maxAttempts = 5;
    std::chrono::milliseconds initialDelay{200};
    double backoffMultiplier = 2.0;
    std::chrono::milliseconds maxDelay{5000};
};

namespace detail {
inline std::chrono::milliseconds withJitter(std::chrono::milliseconds delay) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<long long> dist(0, std::max<long long>(1, delay.count() / 2));
    return delay + std::chrono::milliseconds(dist(rng));
}
}  // namespace detail

// Runs fn() and returns its result. On exception, isTransient(exception) is
// consulted: if it returns false, or this was the last allowed attempt, the
// exception propagates immediately. Otherwise sleeps for an exponentially
// growing, jittered backoff and retries. Intended for transient Oracle
// connectivity errors (connection lost, timeout); auth/permission failures
// should be classified as non-transient by the caller's predicate.
template <typename Fn, typename IsTransient>
auto runWithRetry(Fn&& fn, IsTransient&& isTransient, const RetryOptions& options = {})
    -> decltype(fn()) {
    std::chrono::milliseconds delay = options.initialDelay;

    for (int attempt = 1; attempt <= options.maxAttempts; ++attempt) {
        try {
            return fn();
        } catch (const std::exception& e) {
            const bool lastAttempt = (attempt == options.maxAttempts);
            if (lastAttempt || !isTransient(e)) {
                throw;
            }
            std::this_thread::sleep_for(detail::withJitter(delay));
            delay = std::min(options.maxDelay,
                              std::chrono::milliseconds(static_cast<long long>(
                                  static_cast<double>(delay.count()) * options.backoffMultiplier)));
        }
    }
    // Unreachable: the loop above always either returns or throws.
    throw std::runtime_error("runWithRetry: exhausted attempts without return or throw");
}

}  // namespace dbscanner::db
