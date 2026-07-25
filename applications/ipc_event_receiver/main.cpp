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

    if (busName == nullptr) {
        std::cerr << "usage: " << argv[0] << " --bus <name>\n";
        return 2;
    }

    std::string error;
    elap::ipc::MessageQueueEventBus bus;
    if (!bus.create(busName, 4, 1024, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << "ready\n";
    std::cout.flush();

    elap::ipc::Event event;
    if (!bus.receive(event, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << event.topic << '=' << event.payload << '\n';
    return 0;
}
