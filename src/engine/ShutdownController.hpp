#pragma once

#include <atomic>

namespace dbscanner::engine {

// Each instance owns its own stop flag, so unit tests can create independent
// controllers and call requestStop() without touching real OS signal
// dispositions or interfering with other tests. Only the instance that calls
// install() (normally exactly one, from main()) gets wired to SIGINT/SIGTERM,
// via a static pointer the signal handler trampoline dispatches through.
class ShutdownController {
public:
    ShutdownController() = default;
    ~ShutdownController();

    ShutdownController(const ShutdownController&) = delete;
    ShutdownController& operator=(const ShutdownController&) = delete;

    // Registers SIGINT/SIGTERM handlers for the current process, routed to
    // this instance. Safe to skip in unit tests that just call requestStop().
    void install();

    bool stopRequested() const { return stopFlag_.load(std::memory_order_relaxed); }

    // Test/manual hook to simulate an interruption without sending a real signal.
    void requestStop() { stopFlag_.store(true, std::memory_order_relaxed); }

private:
    static void handleSignal(int signum);

    std::atomic<bool> stopFlag_{false};

    // Signal-safe dispatch target; only valid while this instance is alive
    // and after install() has been called.
    static std::atomic<ShutdownController*> activeInstance_;
};

}  // namespace dbscanner::engine
