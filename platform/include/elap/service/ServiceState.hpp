#pragma once

namespace elap::service {

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

const char* toString(ServiceState state);

} // namespace elap::service
