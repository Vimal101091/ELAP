#pragma once

namespace elap::logging {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, const char* component, const char* message) = 0;
};

const char* toString(LogLevel level);
bool parseLogLevel(const char* text, LogLevel& level);

} // namespace elap::logging
