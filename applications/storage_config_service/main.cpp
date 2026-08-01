#include "elap/config/DatabaseConfiguration.hpp"
#include "elap/logging/ConsoleLogger.hpp"
#include "elap/service/IService.hpp"
#include "elap/service/ServiceApplication.hpp"
#include "elap/threading/ThreadManager.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

class StorageConfigService final : public elap::service::IService {
public:
    const char* name() const override { return "storage_config_service"; }

    bool initialize() override
    {
        std::string error;
        const char* dbEnv = std::getenv("ELAP_STORAGE_CONFIG_DB");
        const std::string dbPath = dbEnv ? dbEnv : "/tmp/elap_storage_config.db";

        if (!config_.open(dbPath, &error)) {
            logger_.log(elap::logging::LogLevel::Error, name(), error.c_str());
            return false;
        }

        const char* seedEnv = std::getenv("ELAP_STORAGE_CONFIG_SEED");
        if (seedEnv) {
            if (!config_.loadFromFile(seedEnv, &error)) {
                logger_.log(elap::logging::LogLevel::Warning, name(), error.c_str());
            }
        }

        workerCount_ = config_.getInt("worker.count", 1);
        heartbeatMs_ = config_.getInt("heartbeat.interval_ms", 1000);

        logger_.log(elap::logging::LogLevel::Info, name(), "configuration loaded from database");
        return true;
    }

    bool start() override
    {
        for (int i = 0; i < workerCount_; ++i) {
            const std::string workerName = "heartbeat_" + std::to_string(i);
            const bool started = threads_.startThread(workerName,
                [this, workerName](elap::threading::StopToken stopToken) {
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
        threads_.requestStop();
    }

    void deinitialize() override
    {
        threads_.joinAll();
        config_.close();
    }

private:
    elap::config::DatabaseConfiguration config_;
    elap::logging::ConsoleLogger logger_;
    elap::threading::ThreadManager threads_ {&logger_, "storage_config_service"};
    int workerCount_ {1};
    int heartbeatMs_ {1000};
};

int main(int argc, char** argv)
{
    StorageConfigService service;
    elap::service::ServiceApplication app;
    return app.run(service, argc, argv);
}
