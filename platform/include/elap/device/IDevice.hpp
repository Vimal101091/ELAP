#pragma once

#include <string>

namespace elap::device {

enum class DeviceState {
    Closed,
    Open,
    Failed
};

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual const char* name() const = 0;
    virtual bool open(std::string* errorMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual DeviceState state() const = 0;
};

const char* toString(DeviceState state);

} // namespace elap::device
