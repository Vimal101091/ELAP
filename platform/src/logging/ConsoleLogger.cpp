#include "elap/logging/ConsoleLogger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace elap::logging {
namespace {

std::string utcTimestamp()
{
    using Clock = std::chrono::system_clock;

    const auto now = Clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = Clock::to_time_t(now);

    std::tm tm {};
    gmtime_r(&time, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << milliseconds.count() << 'Z';
    return stream.str();
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

} // namespace

ConsoleLogger::ConsoleLogger(LogLevel minimumLevel)
    : minimumLevel_(minimumLevel)
{
}

void ConsoleLogger::setMinimumLevel(LogLevel level)
{
    const std::lock_guard lock(mutex_);
    minimumLevel_ = level;
}

LogLevel ConsoleLogger::minimumLevel() const
{
    const std::lock_guard lock(mutex_);
    return minimumLevel_;
}

void ConsoleLogger::log(LogLevel level, const char* component, const char* message)
{
    const std::lock_guard lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(minimumLevel_)) {
        return;
    }

    auto& stream = level >= LogLevel::Error ? std::cerr : std::cout;
    stream << utcTimestamp() << ' ' << toString(level) << ' '
           << (component == nullptr ? "unknown" : component) << ' '
           << (message == nullptr ? "" : message) << '\n';
    stream.flush();
}

const char* toString(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Critical:
        return "CRITICAL";
    }
    return "UNKNOWN";
}

bool parseLogLevel(const char* text, LogLevel& level)
{
    if (text == nullptr) {
        return false;
    }

    const auto value = lower(text);
    if (value == "trace") {
        level = LogLevel::Trace;
        return true;
    }
    if (value == "debug") {
        level = LogLevel::Debug;
        return true;
    }
    if (value == "info") {
        level = LogLevel::Info;
        return true;
    }
    if (value == "warning" || value == "warn") {
        level = LogLevel::Warning;
        return true;
    }
    if (value == "error") {
        level = LogLevel::Error;
        return true;
    }
    if (value == "critical" || value == "fatal") {
        level = LogLevel::Critical;
        return true;
    }
    return false;
}

} // namespace elap::logging
