#pragma once

#include "elap/device/IDevice.hpp"

#include <string>

namespace elap::device {

enum class GpioDirection {
    In,
    Out
};

class GpioPin final : public IDevice {
public:
    GpioPin(std::string name, int pin, GpioDirection direction,
            std::string gpioRoot = "/sys/class/gpio");
    ~GpioPin() override;

    GpioPin(const GpioPin&) = delete;
    GpioPin& operator=(const GpioPin&) = delete;

    const char* name() const override;
    bool open(std::string* errorMessage = nullptr) override;
    void close() override;
    DeviceState state() const override;

    bool writeValue(bool high, std::string* errorMessage = nullptr);
    bool readValue(bool& high, std::string* errorMessage = nullptr) const;

    int pin() const;
    std::string pinPath() const;

private:
    bool writeControlFile(const std::string& fileName, const std::string& value,
                          std::string* errorMessage) const;
    bool writePinFile(const std::string& fileName, const std::string& value,
                      std::string* errorMessage) const;

    std::string name_;
    int pin_;
    GpioDirection direction_;
    std::string gpioRoot_;
    DeviceState state_ {DeviceState::Closed};
    bool exportedByThisInstance_ {false};
};

} // namespace elap::device
