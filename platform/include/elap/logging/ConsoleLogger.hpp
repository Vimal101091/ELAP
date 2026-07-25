#pragma once

#include "elap/logging/ILogger.hpp"

#include <mutex>

namespace elap::logging {

class ConsoleLogger final : public ILogger {
public:
    explicit ConsoleLogger(LogLevel minimumLevel = LogLevel::Info);

    void setMinimumLevel(LogLevel level);
    LogLevel minimumLevel() const;

    void log(LogLevel level, const char* component, const char* message) override;

private:
    mutable std::mutex mutex_;
    LogLevel minimumLevel_;
};

} // namespace elap::logging
