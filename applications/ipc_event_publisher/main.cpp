#include "elap/ipc/EventBus.hpp"

#include <iostream>
#include <string>

namespace {

const char* argumentValue(int argc, char** argv, const char* name)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (argument == name && index + 1 < argc) {
            return argv[index + 1];
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    const char* busName = argumentValue(argc, argv, "--bus");
    const char* topic = argumentValue(argc, argv, "--topic");
    const char* payload = argumentValue(argc, argv, "--payload");

    if (busName == nullptr || topic == nullptr || payload == nullptr) {
        std::cerr << "usage: " << argv[0]
                  << " --bus <name> --topic <topic> --payload <text>\n";
        return 2;
    }

    std::string error;
    elap::ipc::MessageQueueEventBus bus;
    if (!bus.open(busName, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (!bus.publish({topic, payload}, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    return 0;
}
