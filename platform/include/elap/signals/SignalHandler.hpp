#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace elap::signals {

class SignalHandler {
public:
    static bool install();
    static bool shutdownRequested();
    static void clearShutdownRequest();
    static void waitForShutdown(std::condition_variable& cv, std::mutex& mutex);
    static void notifyShutdownRequested();

private:
    static void handleSignal(int signalNumber);

    static inline std::atomic_bool shutdownRequested_{false};
};

} // namespace elap::signals
