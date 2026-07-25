#include "elap/ipc/UnixSocket.hpp"

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
    const char* socketPath = argumentValue(argc, argv, "--socket");
    const char* message = argumentValue(argc, argv, "--message");

    if (socketPath == nullptr || message == nullptr) {
        std::cerr << "usage: " << argv[0] << " --socket <path> --message <text>\n";
        return 2;
    }

    std::string error;
    auto connection = elap::ipc::UnixSocketClient::connect(socketPath, &error);
    if (!connection.isOpen()) {
        std::cerr << error << '\n';
        return 1;
    }

    if (!connection.sendMessage(message, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::string response;
    if (!connection.receiveMessage(response, 64 * 1024, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << response << '\n';
    return 0;
}
