#include "elap/ipc/UnixSocket.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <socket_path>\n";
        return 2;
    }

    std::string error;
    elap::ipc::UnixSocketServer server;
    if (!server.listen(argv[1], 1, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    auto connection = server.accept(&error);
    if (!connection.isOpen()) {
        std::cerr << error << '\n';
        return 1;
    }

    std::string request;
    if (!connection.receiveMessage(request, 64 * 1024, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (!connection.sendMessage("echo:" + request, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    return 0;
}
