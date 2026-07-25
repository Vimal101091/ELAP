#include "elap/signals/SignalHandler.hpp"

#include <csignal>

namespace elap::signals {

bool SignalHandler::install()
{
    struct sigaction action {};
    action.sa_handler = &SignalHandler::handleSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    return sigaction(SIGINT, &action, nullptr) == 0
        && sigaction(SIGTERM, &action, nullptr) == 0;
}

bool SignalHandler::shutdownRequested()
{
    return shutdownRequested_.load();
}

void SignalHandler::clearShutdownRequest()
{
    shutdownRequested_.store(false);
}

void SignalHandler::waitForShutdown(std::condition_variable& cv, std::mutex& mutex)
{
    std::unique_lock lock(mutex);
    cv.wait(lock, [] { return shutdownRequested(); });
}

void SignalHandler::notifyShutdownRequested()
{
    shutdownRequested_.store(true);
}

void SignalHandler::handleSignal(int)
{
    shutdownRequested_.store(true);
}

} // namespace elap::signals
