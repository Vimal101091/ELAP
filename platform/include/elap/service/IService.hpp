#pragma once

namespace elap::service {

class IService {
public:
    virtual ~IService() = default;

    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void deinitialize() = 0;
    virtual const char* name() const = 0;
};

} // namespace elap::service
