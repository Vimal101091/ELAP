#pragma once

#include "elap/device/IDevice.hpp"

#include <memory>
#include <string>
#include <vector>

namespace elap::device {

class DeviceManager {
public:
    bool add(std::unique_ptr<IDevice> device, std::string* errorMessage = nullptr);
    bool openAll(std::string* errorMessage = nullptr);
    void closeAll();

    IDevice* find(const std::string& name);
    const IDevice* find(const std::string& name) const;
    std::size_t count() const;

private:
    std::vector<std::unique_ptr<IDevice>> devices_;
};

} // namespace elap::device
