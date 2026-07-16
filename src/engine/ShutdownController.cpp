#include "engine/ShutdownController.hpp"

#include <csignal>

namespace dbscanner::engine {

std::atomic<ShutdownController*> ShutdownController::activeInstance_{nullptr};

void ShutdownController::install() {
    activeInstance_.store(this, std::memory_order_relaxed);

    struct sigaction action {};
    action.sa_handler = &ShutdownController::handleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
}

ShutdownController::~ShutdownController() {
    ShutdownController* expected = this;
    activeInstance_.compare_exchange_strong(expected, nullptr, std::memory_order_relaxed);
}

void ShutdownController::handleSignal(int /*signum*/) {
    if (ShutdownController* instance = activeInstance_.load(std::memory_order_relaxed)) {
        instance->stopFlag_.store(true, std::memory_order_relaxed);
    }
}

}  // namespace dbscanner::engine
