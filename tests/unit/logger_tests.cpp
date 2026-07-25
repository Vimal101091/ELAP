#include "elap/logging/ILogger.hpp"
#include "elap/service/ServiceApplication.hpp"

#include <cassert>
#include <string>

namespace {

void runLoggerTests()
{
    elap::logging::LogLevel level = elap::logging::LogLevel::Info;

    assert(elap::logging::parseLogLevel("trace", level));
    assert(level == elap::logging::LogLevel::Trace);
    assert(elap::logging::parseLogLevel("warn", level));
    assert(level == elap::logging::LogLevel::Warning);
    assert(elap::logging::parseLogLevel("fatal", level));
    assert(level == elap::logging::LogLevel::Critical);
    assert(!elap::logging::parseLogLevel("verbose", level));

    assert(elap::logging::toString(elap::logging::LogLevel::Error) == std::string("ERROR"));
    assert(elap::service::toString(elap::service::ServiceState::Running) == std::string("Running"));
}

} // namespace

struct LoggerTestRunner {
    LoggerTestRunner()
    {
        runLoggerTests();
    }
} loggerTestRunner;
