#include <gtest/gtest.h>

#include <stdexcept>

#include "db/RetryPolicy.hpp"

using namespace dbscanner::db;

namespace {
struct TransientError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct FatalError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

RetryOptions fastOptions() {
    RetryOptions options;
    options.maxAttempts = 4;
    options.initialDelay = std::chrono::milliseconds(1);
    options.maxDelay = std::chrono::milliseconds(2);
    return options;
}
}  // namespace

TEST(RetryPolicy, SucceedsAfterTransientFailures) {
    int attempts = 0;
    const int result = runWithRetry(
        [&] {
            ++attempts;
            if (attempts < 3) throw TransientError("try again");
            return 42;
        },
        [](const std::exception&) { return true; }, fastOptions());

    EXPECT_EQ(result, 42);
    EXPECT_EQ(attempts, 3);
}

TEST(RetryPolicy, RethrowsImmediatelyOnNonTransientError) {
    int attempts = 0;
    EXPECT_THROW(
        runWithRetry(
            [&]() -> int {
                ++attempts;
                throw FatalError("auth failed");
            },
            [](const std::exception& e) { return dynamic_cast<const TransientError*>(&e) != nullptr; },
            fastOptions()),
        FatalError);

    EXPECT_EQ(attempts, 1);
}

TEST(RetryPolicy, RethrowsAfterExhaustingAttempts) {
    int attempts = 0;
    RetryOptions options = fastOptions();
    options.maxAttempts = 3;

    EXPECT_THROW(
        runWithRetry(
            [&]() -> int {
                ++attempts;
                throw TransientError("still failing");
            },
            [](const std::exception&) { return true; }, options),
        TransientError);

    EXPECT_EQ(attempts, 3);
}
