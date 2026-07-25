#include "elap/ipc/SharedMemory.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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
    const char* regionName = argumentValue(argc, argv, "--name");
    const char* message = argumentValue(argc, argv, "--message");
    const char* holdMsText = argumentValue(argc, argv, "--hold-ms");

    if (regionName == nullptr || message == nullptr) {
        std::cerr << "usage: " << argv[0]
                  << " --name <name> --message <text> [--hold-ms <ms>]\n";
        return 2;
    }

    int holdMs = 5000;
    if (holdMsText != nullptr) {
        holdMs = std::stoi(holdMsText);
        if (holdMs < 0) {
            holdMs = 0;
        }
    }

    std::string error;
    elap::ipc::SharedMemoryRegion region;
    if (!region.create(regionName, 4096, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    const std::string payload = message;
    if (!region.write(0, payload.data(), payload.size(), &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << "ready\n";
    std::cout.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
    return 0;
}
