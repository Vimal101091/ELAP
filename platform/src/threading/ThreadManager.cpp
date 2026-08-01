#include "elap/threading/ThreadManager.hpp"

#include <exception>
#include <thread>
#include <utility>

namespace elap::threading {

StopToken::StopToken()
    : stopRequested_(std::make_shared<std::atomic_bool>(false))
{
}

StopToken::StopToken(std::shared_ptr<std::atomic_bool> stopRequested)
    : stopRequested_(std::move(stopRequested))
{
}

bool StopToken::stop_requested() const
{
    return stopRequested_ != nullptr && stopRequested_->load();
}

ThreadManager::ThreadManager(logging::ILogger* logger, std::string component)
    : logger_(logger)
    , component_(std::move(component))
{
}

ThreadManager::~ThreadManager()
{
    requestStop();
    joinAll();
}

bool ThreadManager::startThread(const std::string& name, Worker worker)
{
    if (!worker) {
        return false;
    }

    const std::lock_guard lock(mutex_);
    for (const auto& managedThread : threads_) {
        if (managedThread.name == name) {
            return false;
        }
    }

    auto stopRequested = std::make_shared<std::atomic_bool>(false);
    threads_.reserve(threads_.size() + 1);

    auto threadName = name;
    std::thread thread([this, threadName, stopToken = StopToken(stopRequested),
                        worker = std::move(worker)]() mutable {
        if (logger_ != nullptr) {
            logger_->log(logging::LogLevel::Info, component_.c_str(),
                         ("thread started: " + threadName).c_str());
        }

        try {
            worker(stopToken);
        } catch (const std::exception& exception) {
            if (logger_ != nullptr) {
                logger_->log(logging::LogLevel::Error, component_.c_str(), exception.what());
            }
        } catch (...) {
            if (logger_ != nullptr) {
                logger_->log(logging::LogLevel::Error, component_.c_str(),
                             "thread terminated with unknown exception");
            }
        }

        if (logger_ != nullptr) {
            logger_->log(logging::LogLevel::Info, component_.c_str(),
                         ("thread stopped: " + threadName).c_str());
        }
    });

    threads_.push_back({std::move(threadName), std::move(stopRequested), std::move(thread)});
    return true;
}

void ThreadManager::requestStop()
{
    const std::lock_guard lock(mutex_);
    for (auto& managedThread : threads_) {
        managedThread.stopRequested->store(true);
    }
}

void ThreadManager::joinAll()
{
    std::vector<ManagedThread> threads;
    {
        const std::lock_guard lock(mutex_);
        threads.swap(threads_);
    }

    for (auto& managedThread : threads) {
        if (managedThread.thread.joinable()) {
            if (managedThread.thread.get_id() == std::this_thread::get_id()) {
                managedThread.thread.detach();
            } else {
                managedThread.thread.join();
            }
        }
    }
}

std::size_t ThreadManager::threadCount() const
{
    const std::lock_guard lock(mutex_);
    return threads_.size();
}

} // namespace elap::threading
