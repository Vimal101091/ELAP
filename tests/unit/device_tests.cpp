#include "elap/device/DeviceManager.hpp"
#include "elap/device/FileDevice.hpp"
#include "elap/device/GpioPin.hpp"

#include <cassert>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

namespace {

std::string testPath(const char* name)
{
    return "/tmp/elap_" + std::string(name) + "_" + std::to_string(::getpid());
}

void runFileDeviceRoundTripTest()
{
    const auto path = testPath("file_device");
    {
        std::ofstream seed(path);
        seed << "00000";
    }

    elap::device::FileDevice device("loopback", path, O_RDWR);
    std::string error;
    assert(device.state() == elap::device::DeviceState::Closed);
    assert(device.open(&error));
    assert(device.state() == elap::device::DeviceState::Open);

    const std::string payload = "abc";
    assert(device.write(payload.data(), payload.size(), &error));
    assert(::lseek(device.nativeHandle(), 0, SEEK_SET) == 0);

    char buffer[4] {};
    std::size_t bytesRead = 0;
    assert(device.read(buffer, 3, bytesRead, &error));
    assert(bytesRead == 3);
    assert(std::string(buffer, bytesRead) == payload);

    device.close();
    assert(device.state() == elap::device::DeviceState::Closed);
    std::filesystem::remove(path);
}

void runDeviceManagerTest()
{
    const auto path = testPath("managed_device");
    {
        std::ofstream seed(path);
        seed << "ready";
    }

    elap::device::DeviceManager manager;
    std::string error;
    assert(manager.add(std::make_unique<elap::device::FileDevice>("dev0", path, O_RDONLY),
                       &error));
    assert(!manager.add(std::make_unique<elap::device::FileDevice>("dev0", path, O_RDONLY),
                        &error));
    assert(manager.count() == 1);
    assert(manager.find("dev0") != nullptr);
    assert(manager.openAll(&error));
    assert(manager.find("dev0")->state() == elap::device::DeviceState::Open);
    manager.closeAll();
    assert(manager.find("dev0")->state() == elap::device::DeviceState::Closed);
    std::filesystem::remove(path);
}

void runGpioPinSysfsTest()
{
    const auto root = testPath("gpio_root");
    const auto pinPath = root + "/gpio17";
    std::filesystem::create_directories(pinPath);
    {
        std::ofstream(root + "/export");
        std::ofstream(root + "/unexport");
        std::ofstream(pinPath + "/direction");
        std::ofstream(pinPath + "/value") << "0";
    }

    elap::device::GpioPin pin("status_led", 17, elap::device::GpioDirection::Out, root);
    std::string error;
    assert(pin.open(&error));
    assert(pin.state() == elap::device::DeviceState::Open);
    assert(pin.writeValue(true, &error));

    bool high = false;
    assert(pin.readValue(high, &error));
    assert(high);

    pin.close();
    std::filesystem::remove_all(root);
}

void runDeviceTests()
{
    runFileDeviceRoundTripTest();
    runDeviceManagerTest();
    runGpioPinSysfsTest();
}

} // namespace

struct DeviceTestRunner {
    DeviceTestRunner()
    {
        runDeviceTests();
    }
} deviceTestRunner;
