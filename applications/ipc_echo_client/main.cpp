#include "elap/ipc/UnixSocket.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <socket_path> <message>\n";
        return 2;
    }

    std::string error;
    auto connection = elap::ipc::UnixSocketClient::connect(argv[1], &error);
    if (!connection.isOpen()) {
        std::cerr << error << '\n';
        return 1;
    }

    if (!connection.sendMessage(argv[2], &error)) {
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
