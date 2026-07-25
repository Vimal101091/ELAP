#pragma once

#include "elap/logging/ILogger.hpp"

#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace elap::threading {

class StopToken {
public:
    StopToken();
    explicit StopToken(std::shared_ptr<std::atomic_bool> stopRequested);

    bool stop_requested() const;

private:
    std::shared_ptr<std::atomic_bool> stopRequested_;
};

class ThreadManager {
public:
    using Worker = std::function<void(StopToken)>;

    explicit ThreadManager(logging::ILogger* logger = nullptr,
                           std::string component = "thread_manager");
    ~ThreadManager();

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    bool startThread(const std::string& name, Worker worker);
    void requestStop();
    void joinAll();
    std::size_t threadCount() const;

private:
    struct ManagedThread {
        std::string name;
        std::shared_ptr<std::atomic_bool> stopRequested;
        std::thread thread;
    };

    logging::ILogger* logger_;
    std::string component_;
    mutable std::mutex mutex_;
    std::vector<ManagedThread> threads_;
};

} // namespace elap::threading
