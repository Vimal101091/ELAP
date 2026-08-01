#include "elap/device/DeviceManager.hpp"

#include <utility>

namespace elap::device {
namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

bool DeviceManager::add(std::unique_ptr<IDevice> device, std::string* errorMessage)
{
    if (!device) {
        setError(errorMessage, "device is null");
        return false;
    }
    if (find(device->name()) != nullptr) {
        setError(errorMessage, "device already registered: " + std::string(device->name()));
        return false;
    }
    devices_.push_back(std::move(device));
    return true;
}

bool DeviceManager::openAll(std::string* errorMessage)
{
    std::vector<IDevice*> openedDevices;
    openedDevices.reserve(devices_.size());

    for (auto& device : devices_) {
        if (!device->open(errorMessage)) {
            for (auto iterator = openedDevices.rbegin();
                 iterator != openedDevices.rend();
                 ++iterator) {
                (*iterator)->close();
            }
            return false;
        }
        openedDevices.push_back(device.get());
    }
    return true;
}

void DeviceManager::closeAll()
{
    for (auto& device : devices_) {
        device->close();
    }
}

IDevice* DeviceManager::find(const std::string& name)
{
    for (auto& device : devices_) {
        if (device->name() == name) {
            return device.get();
        }
    }
    return nullptr;
}

const IDevice* DeviceManager::find(const std::string& name) const
{
    for (const auto& device : devices_) {
        if (device->name() == name) {
            return device.get();
        }
    }
    return nullptr;
}

std::size_t DeviceManager::count() const
{
    return devices_.size();
}

} // namespace elap::device
