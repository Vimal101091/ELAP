#include "elap/config/KeyValueConfiguration.hpp"
#include "elap/ipc/UnixSocket.hpp"
#include "elap/logging/ConsoleLogger.hpp"
#include "elap/service/IService.hpp"
#include "elap/service/ServiceApplication.hpp"
#include "elap/threading/ThreadManager.hpp"

#include <chrono>
#include <filesystem>
#include <string>

namespace {

std::string selectConfigPath(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (argument == "--config" && index + 1 < argc) {
            return argv[index + 1];
        }
    }

    if (std::filesystem::exists("./config/sample_ipc_server.conf")) {
        return "./config/sample_ipc_server.conf";
    }
    return "/etc/elap/sample_ipc_server.conf";
}

class SampleIpcServer final : public elap::service::IService {
public:
    SampleIpcServer(int argc, char** argv)
        : configPath_(selectConfigPath(argc, argv))
        , threads_(&logger_, "sample_ipc_server")
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

        socketPath_ = configuration_.getString("ipc.socket_path", "/tmp/elap_sample_ipc_server.sock");
        return true;
    }

    bool start() override
    {
        std::string error;
        if (!server_.listen(socketPath_, 8, &error)) {
            logger_.log(elap::logging::LogLevel::Error, name(), error.c_str());
            return false;
        }

        logger_.log(elap::logging::LogLevel::Info, name(),
                    ("listening on " + socketPath_).c_str());

        return threads_.startThread("ipc_listener", [this](elap::threading::StopToken stopToken) {
            while (!stopToken.stop_requested()) {
                elap::ipc::UnixSocketConnection connection;
                std::string error;
                const bool accepted = server_.tryAccept(connection,
                                                        std::chrono::milliseconds(100),
                                                        &error);
                if (!accepted) {
                    if (!error.empty()) {
                        logger_.log(elap::logging::LogLevel::Warning, name(), error.c_str());
                    }
                    continue;
                }

                std::string request;
                if (!connection.receiveMessage(request, 64 * 1024, &error)) {
                    logger_.log(elap::logging::LogLevel::Warning, name(), error.c_str());
                    continue;
                }

                logger_.log(elap::logging::LogLevel::Info, name(),
                            ("received request: " + request).c_str());

                if (!connection.sendMessage("sample_ipc_server:" + request, &error)) {
                    logger_.log(elap::logging::LogLevel::Warning, name(), error.c_str());
                }
            }
        });
    }

    void stop() override
    {
        logger_.log(elap::logging::LogLevel::Info, name(), "requesting IPC shutdown");
        server_.close();
        threads_.requestStop();
        threads_.joinAll();
    }

    void deinitialize() override
    {
        logger_.log(elap::logging::LogLevel::Info, name(), "deinitialized");
    }

    const char* name() const override
    {
        return "sample_ipc_server";
    }

private:
    std::string configPath_;
    std::string socketPath_ {"/tmp/elap_sample_ipc_server.sock"};
    elap::config::KeyValueConfiguration configuration_;
    elap::logging::ConsoleLogger logger_;
    elap::ipc::UnixSocketServer server_;
    elap::threading::ThreadManager threads_;
};

} // namespace

int main(int argc, char** argv)
{
    SampleIpcServer service(argc, argv);
    elap::service::ServiceApplication application;
    return application.run(service, argc, argv);
}
