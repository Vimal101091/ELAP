#include "elap/service/ServiceApplication.hpp"

#include "elap/config/KeyValueConfiguration.hpp"
#include "elap/service/IService.hpp"
#include "elap/signals/SignalHandler.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <string>

namespace elap::service {
namespace {

struct ConfigSelection {
    std::string path;
    bool explicitPath {false};
};

ConfigSelection selectConfigPath(int argc, char** argv, const char* serviceName)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (argument == "--config" && index + 1 < argc) {
            return {argv[index + 1], true};
        }
    }

    const std::string localPath = "./config/" + std::string(serviceName) + ".conf";
    if (std::filesystem::exists(localPath)) {
        return {localPath, false};
    }

    return {"/etc/elap/" + std::string(serviceName) + ".conf", false};
}

void configureLogger(logging::ConsoleLogger& logger,
                     const config::KeyValueConfiguration& configuration)
{
    logging::LogLevel logLevel = logging::LogLevel::Info;
    const auto configuredLevel = configuration.getString("log.level", "info");
    if (logging::parseLogLevel(configuredLevel.c_str(), logLevel)) {
        logger.setMinimumLevel(logLevel);
    }
}

void logException(logging::ConsoleLogger& logger,
                  const char* serviceName,
                  const char* operation,
                  const std::exception& exception)
{
    logger.log(logging::LogLevel::Error,
               serviceName,
               (std::string(operation) + " threw exception: " + exception.what()).c_str());
}

void logUnknownException(logging::ConsoleLogger& logger,
                         const char* serviceName,
                         const char* operation)
{
    logger.log(logging::LogLevel::Error,
               serviceName,
               (std::string(operation) + " threw unknown exception").c_str());
}

} // namespace

int ServiceApplication::run(IService& service, int argc, char** argv)
{
    signals::SignalHandler::clearShutdownRequest();

    config::KeyValueConfiguration configuration;
    const auto configSelection = selectConfigPath(argc, argv, service.name());
    std::string configError;
    const bool configLoaded = configuration.loadFromFile(configSelection.path, &configError);
    if (!configLoaded && configSelection.explicitPath) {
        logger_.log(logging::LogLevel::Error, service.name(), configError.c_str());
        setState(ServiceState::Failed);
        return 2;
    }
    if (configLoaded) {
        configureLogger(logger_, configuration);
        logger_.log(logging::LogLevel::Info, service.name(),
                    ("loaded configuration: " + configSelection.path).c_str());
    } else {
        logger_.log(logging::LogLevel::Warning, service.name(),
                    ("using defaults; configuration not found: " + configSelection.path).c_str());
    }

    if (!signals::SignalHandler::install()) {
        logger_.log(logging::LogLevel::Error, service.name(), "failed to install signal handlers");
        setState(ServiceState::Failed);
        return 3;
    }

    setState(ServiceState::Initializing);
    logger_.log(logging::LogLevel::Info, service.name(), "initializing service");
    bool initialized = false;
    try {
        initialized = service.initialize();
    } catch (const std::exception& exception) {
        logException(logger_, service.name(), "service initialization", exception);
        setState(ServiceState::Failed);
        return 4;
    } catch (...) {
        logUnknownException(logger_, service.name(), "service initialization");
        setState(ServiceState::Failed);
        return 4;
    }

    if (!initialized) {
        logger_.log(logging::LogLevel::Error, service.name(), "service initialization failed");
        try {
            service.deinitialize();
        } catch (const std::exception& exception) {
            logException(logger_, service.name(), "service deinitialization", exception);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 4;
    }

    setState(ServiceState::Initialized);
    setState(ServiceState::Starting);
    logger_.log(logging::LogLevel::Info, service.name(), "starting service");
    bool started = false;
    try {
        started = service.start();
    } catch (const std::exception& exception) {
        logException(logger_, service.name(), "service start", exception);
        try {
            service.deinitialize();
        } catch (const std::exception& deinitializeException) {
            logException(logger_, service.name(), "service deinitialization", deinitializeException);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 5;
    } catch (...) {
        logUnknownException(logger_, service.name(), "service start");
        try {
            service.deinitialize();
        } catch (const std::exception& exception) {
            logException(logger_, service.name(), "service deinitialization", exception);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 5;
    }

    if (!started) {
        logger_.log(logging::LogLevel::Error, service.name(), "service start failed");
        try {
            service.deinitialize();
        } catch (const std::exception& exception) {
            logException(logger_, service.name(), "service deinitialization", exception);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 5;
    }

    setState(ServiceState::Running);
    logger_.log(logging::LogLevel::Info, service.name(), "service running");
    {
        std::unique_lock lock(mutex_);
        shutdownCv_.wait_for(lock, std::chrono::milliseconds(250), [] {
            return signals::SignalHandler::shutdownRequested();
        });
        while (!signals::SignalHandler::shutdownRequested()) {
            shutdownCv_.wait_for(lock, std::chrono::milliseconds(250));
        }
    }

    setState(ServiceState::Stopping);
    logger_.log(logging::LogLevel::Info, service.name(), "stopping service");
    try {
        service.stop();
    } catch (const std::exception& exception) {
        logException(logger_, service.name(), "service stop", exception);
        try {
            service.deinitialize();
        } catch (const std::exception& deinitializeException) {
            logException(logger_, service.name(), "service deinitialization", deinitializeException);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 6;
    } catch (...) {
        logUnknownException(logger_, service.name(), "service stop");
        try {
            service.deinitialize();
        } catch (const std::exception& exception) {
            logException(logger_, service.name(), "service deinitialization", exception);
        } catch (...) {
            logUnknownException(logger_, service.name(), "service deinitialization");
        }
        setState(ServiceState::Failed);
        return 6;
    }

    try {
        service.deinitialize();
    } catch (const std::exception& exception) {
        logException(logger_, service.name(), "service deinitialization", exception);
        setState(ServiceState::Failed);
        return 7;
    } catch (...) {
        logUnknownException(logger_, service.name(), "service deinitialization");
        setState(ServiceState::Failed);
        return 7;
    }
    setState(ServiceState::Stopped);
    logger_.log(logging::LogLevel::Info, service.name(), "service stopped");
    return 0;
}

ServiceState ServiceApplication::state() const
{
    return state_.load();
}

std::string ServiceApplication::parseConfigPath(int argc, char** argv, const char* serviceName) const
{
    return selectConfigPath(argc, argv, serviceName).path;
}

void ServiceApplication::setState(ServiceState state)
{
    state_.store(state);
}

const char* toString(ServiceState state)
{
    switch (state) {
    case ServiceState::Created:
        return "Created";
    case ServiceState::Initializing:
        return "Initializing";
    case ServiceState::Initialized:
        return "Initialized";
    case ServiceState::Starting:
        return "Starting";
    case ServiceState::Running:
        return "Running";
    case ServiceState::Stopping:
        return "Stopping";
    case ServiceState::Stopped:
        return "Stopped";
    case ServiceState::Failed:
        return "Failed";
    }
    return "Unknown";
}

} // namespace elap::service
