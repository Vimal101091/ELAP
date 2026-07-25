#pragma once

#include "elap/logging/ConsoleLogger.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

namespace elap::service {

class IService;

enum class ServiceState {
    Created,
    Initializing,
    Initialized,
    Starting,
    Running,
    Stopping,
    Stopped,
    Failed
};

class ServiceApplication {
public:
    int run(IService& service, int argc, char** argv);

    ServiceState state() const;

private:
    std::string parseConfigPath(int argc, char** argv, const char* serviceName) const;
    void setState(ServiceState state);

    mutable std::mutex mutex_;
    std::condition_variable shutdownCv_;
    std::atomic<ServiceState> state_{ServiceState::Created};
    logging::ConsoleLogger logger_;
};

const char* toString(ServiceState state);

} // namespace elap::service
