#include "elap/device/GpioPin.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <utility>

namespace elap::device {
namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

const char* toSysfsDirection(GpioDirection direction)
{
    return direction == GpioDirection::Out ? "out" : "in";
}

bool waitForPath(const std::string& path, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (std::filesystem::exists(path)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);

    return std::filesystem::exists(path);
}

} // namespace

GpioPin::GpioPin(std::string name, int pin, GpioDirection direction, std::string gpioRoot)
    : name_(std::move(name))
    , pin_(pin)
    , direction_(direction)
    , gpioRoot_(std::move(gpioRoot))
{
}

GpioPin::~GpioPin()
{
    close();
}

const char* GpioPin::name() const
{
    return name_.c_str();
}

bool GpioPin::open(std::string* errorMessage)
{
    if (pin_ < 0) {
        state_ = DeviceState::Failed;
        setError(errorMessage, "gpio pin must be non-negative");
        return false;
    }

    const auto path = pinPath();
    if (!std::filesystem::exists(path)) {
        if (!writeControlFile("export", std::to_string(pin_), errorMessage)) {
            state_ = DeviceState::Failed;
            return false;
        }
        exportedByThisInstance_ = true;
        if (!waitForPath(path, std::chrono::milliseconds(250))) {
            writeControlFile("unexport", std::to_string(pin_), nullptr);
            exportedByThisInstance_ = false;
            state_ = DeviceState::Failed;
            setError(errorMessage, "gpio export failed: pin path did not appear");
            return false;
        }
    }

    if (!writePinFile("direction", toSysfsDirection(direction_), errorMessage)) {
        if (exportedByThisInstance_) {
            writeControlFile("unexport", std::to_string(pin_), nullptr);
            exportedByThisInstance_ = false;
        }
        state_ = DeviceState::Failed;
        return false;
    }

    state_ = DeviceState::Open;
    return true;
}

void GpioPin::close()
{
    if (exportedByThisInstance_) {
        writeControlFile("unexport", std::to_string(pin_), nullptr);
        exportedByThisInstance_ = false;
    }
    state_ = DeviceState::Closed;
}

DeviceState GpioPin::state() const
{
    return state_;
}

bool GpioPin::writeValue(bool high, std::string* errorMessage)
{
    if (state_ != DeviceState::Open) {
        setError(errorMessage, "gpio write failed: pin is not open");
        return false;
    }
    if (direction_ != GpioDirection::Out) {
        setError(errorMessage, "gpio write failed: pin direction is input");
        return false;
    }
    return writePinFile("value", high ? "1" : "0", errorMessage);
}

bool GpioPin::readValue(bool& high, std::string* errorMessage) const
{
    high = false;
    if (state_ != DeviceState::Open) {
        setError(errorMessage, "gpio read failed: pin is not open");
        return false;
    }

    std::ifstream stream(pinPath() + "/value");
    if (!stream) {
        setError(errorMessage, "failed to open gpio value file");
        return false;
    }

    char value = '\0';
    stream >> value;
    if (value == '0') {
        high = false;
        return true;
    }
    if (value == '1') {
        high = true;
        return true;
    }

    setError(errorMessage, "invalid gpio value");
    return false;
}

int GpioPin::pin() const
{
    return pin_;
}

std::string GpioPin::pinPath() const
{
    return gpioRoot_ + "/gpio" + std::to_string(pin_);
}

bool GpioPin::writeControlFile(const std::string& fileName, const std::string& value,
                               std::string* errorMessage) const
{
    std::ofstream stream(gpioRoot_ + "/" + fileName);
    if (!stream) {
        setError(errorMessage, "failed to open gpio " + fileName + " file");
        return false;
    }
    stream << value;
    if (!stream) {
        setError(errorMessage, "failed to write gpio " + fileName + " file");
        return false;
    }
    return true;
}

bool GpioPin::writePinFile(const std::string& fileName, const std::string& value,
                           std::string* errorMessage) const
{
    std::ofstream stream(pinPath() + "/" + fileName);
    if (!stream) {
        setError(errorMessage, "failed to open gpio pin " + fileName + " file");
        return false;
    }
    stream << value;
    if (!stream) {
        setError(errorMessage, "failed to write gpio pin " + fileName + " file");
        return false;
    }
    return true;
}

} // namespace elap::device
