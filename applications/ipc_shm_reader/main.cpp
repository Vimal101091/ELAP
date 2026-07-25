#include "elap/ipc/SharedMemory.hpp"

#include <iostream>
#include <string>
#include <vector>

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
    const char* bytesText = argumentValue(argc, argv, "--bytes");

    if (regionName == nullptr || bytesText == nullptr) {
        std::cerr << "usage: " << argv[0] << " --name <name> --bytes <count>\n";
        return 2;
    }

    const auto bytes = static_cast<std::size_t>(std::stoul(bytesText));

    std::string error;
    elap::ipc::SharedMemoryRegion region;
    if (!region.open(regionName, 4096, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::vector<char> buffer(bytes);
    if (!region.read(0, buffer.data(), buffer.size(), &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << std::string(buffer.data(), buffer.size()) << '\n';
    return 0;
}
