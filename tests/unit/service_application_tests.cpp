#include "elap/service/IService.hpp"
#include "elap/service/ServiceApplication.hpp"
#include "elap/signals/SignalHandler.hpp"

#include <cassert>
#include <stdexcept>

namespace {

class TestService final : public elap::service::IService {
public:
    bool initialize() override
    {
        ++initializeCalls;
        if (throwInitialize) {
            throw std::runtime_error("initialize failed");
        }
        return initializeResult;
    }

    bool start() override
    {
        ++startCalls;
        if (throwStart) {
            throw std::runtime_error("start failed");
        }
        if (requestShutdownOnStart) {
            elap::signals::SignalHandler::notifyShutdownRequested();
        }
        return startResult;
    }

    void stop() override
    {
        ++stopCalls;
        if (throwStop) {
            throw std::runtime_error("stop failed");
        }
    }

    void deinitialize() override
    {
        ++deinitializeCalls;
        if (throwDeinitialize) {
            throw std::runtime_error("deinitialize failed");
        }
    }

    const char* name() const override
    {
        return "test_service";
    }

    bool initializeResult {true};
    bool startResult {true};
    bool requestShutdownOnStart {false};
    bool throwInitialize {false};
    bool throwStart {false};
    bool throwStop {false};
    bool throwDeinitialize {false};
    int initializeCalls {0};
    int startCalls {0};
    int stopCalls {0};
    int deinitializeCalls {0};
};

void runServiceApplicationTestsImpl()
{
    {
        TestService service;
        service.throwInitialize = true;

        elap::service::ServiceApplication application;
        assert(application.run(service, 0, nullptr) == 4);
        assert(application.state() == elap::service::ServiceState::Failed);
        assert(service.initializeCalls == 1);
        assert(service.startCalls == 0);
        assert(service.stopCalls == 0);
        assert(service.deinitializeCalls == 0);
    }

    {
        TestService service;
        service.throwStart = true;

        elap::service::ServiceApplication application;
        assert(application.run(service, 0, nullptr) == 5);
        assert(application.state() == elap::service::ServiceState::Failed);
        assert(service.initializeCalls == 1);
        assert(service.startCalls == 1);
        assert(service.stopCalls == 0);
        assert(service.deinitializeCalls == 1);
    }

    {
        TestService service;
        service.requestShutdownOnStart = true;

        elap::service::ServiceApplication application;
        assert(application.run(service, 0, nullptr) == 0);
        assert(application.state() == elap::service::ServiceState::Stopped);
        assert(service.initializeCalls == 1);
        assert(service.startCalls == 1);
        assert(service.stopCalls == 1);
        assert(service.deinitializeCalls == 1);
    }

    {
        TestService service;
        service.requestShutdownOnStart = true;
        service.throwDeinitialize = true;

        elap::service::ServiceApplication application;
        assert(application.run(service, 0, nullptr) == 7);
        assert(application.state() == elap::service::ServiceState::Failed);
        assert(service.stopCalls == 1);
        assert(service.deinitializeCalls == 1);
    }
}

} // namespace

void runServiceApplicationTests()
{
    runServiceApplicationTestsImpl();
}
