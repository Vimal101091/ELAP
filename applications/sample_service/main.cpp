#include "elap/config/KeyValueConfiguration.hpp"
#include "elap/logging/ConsoleLogger.hpp"
#include "elap/service/IService.hpp"
#include "elap/service/ServiceApplication.hpp"
#include "elap/threading/ThreadManager.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::string selectConfigPath(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (argument == "--config" && index + 1 < argc) {
            return argv[index + 1];
        }
    }

    if (std::filesystem::exists("./config/sample_service.conf")) {
        return "./config/sample_service.conf";
    }
    return "/etc/elap/sample_service.conf";
}

class SampleService final : public elap::service::IService {
public:
    SampleService(int argc, char** argv)
        : configPath_(selectConfigPath(argc, argv))
        , threads_(&logger_, "sample_service")
    {
    }

    bool initialize() override
    {
        std::string error;
        if (configuration_.loadFromFile(configPath_, &error)) {
            elap::logging::LogLevel logLevel = elap::logging::LogLevel::Info;
            const auto configuredLevel = configuration_.getString("log.level", "info");
            if (elap::logging::parseLogLevel(configuredLevel.c_str(), logLevel)) {
                logger_.setMinimumLevel(logLevel);
            }
            logger_.log(elap::logging::LogLevel::Info, name(), "configuration loaded");
        } else {
            logger_.log(elap::logging::LogLevel::Warning, name(), error.c_str());
        }

        workerCount_ = configuration_.getInt("worker.count", 1);
        if (workerCount_ < 1) {
            workerCount_ = 1;
        }
        heartbeatMs_ = configuration_.getInt("heartbeat.interval_ms", 1000);
        if (heartbeatMs_ < 100) {
            heartbeatMs_ = 100;
        }
        return true;
    }

    bool start() override
    {
        for (int index = 0; index < workerCount_; ++index) {
            const auto workerName = "heartbeat_" + std::to_string(index);
            const bool started = threads_.startThread(workerName, [this, workerName](elap::threading::StopToken stopToken) {
                while (!stopToken.stop_requested()) {
                    logger_.log(elap::logging::LogLevel::Info, name(),
                                ("heartbeat from " + workerName).c_str());
                    std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatMs_));
                }
            });
            if (!started) {
                return false;
            }
        }
        return true;
    }

    void stop() override
    {
        logger_.log(elap::logging::LogLevel::Info, name(), "requesting worker shutdown");
        threads_.requestStop();
        threads_.joinAll();
    }

    void deinitialize() override
    {
        logger_.log(elap::logging::LogLevel::Info, name(), "deinitialized");
    }

    const char* name() const override
    {
        return "sample_service";
    }

private:
    std::string configPath_;
    elap::config::KeyValueConfiguration configuration_;
    elap::logging::ConsoleLogger logger_;
    elap::threading::ThreadManager threads_;
    int workerCount_ {1};
    int heartbeatMs_ {1000};
};

} // namespace

int main(int argc, char** argv)
{
    SampleService service(argc, argv);
    elap::service::ServiceApplication application;
    return application.run(service, argc, argv);
}
