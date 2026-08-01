#include "elap/device/IDevice.hpp"

namespace elap::device {

const char* toString(DeviceState state)
{
    switch (state) {
    case DeviceState::Closed:
        return "Closed";
    case DeviceState::Open:
        return "Open";
    case DeviceState::Failed:
        return "Failed";
    }
    return "Unknown";
}

} // namespace elap::device
