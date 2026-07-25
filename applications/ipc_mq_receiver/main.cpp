#include "elap/ipc/PosixMessageQueue.hpp"

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
    const char* queueName = argumentValue(argc, argv, "--queue");

    if (queueName == nullptr) {
        std::cerr << "usage: " << argv[0] << " --queue <name>\n";
        return 2;
    }

    std::string error;
    elap::ipc::PosixMessageQueue queue;
    if (!queue.create(queueName, 4, 1024, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << "ready\n";
    std::cout.flush();

    std::string message;
    if (!queue.receiveMessage(message, nullptr, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << message << '\n';
    return 0;
}
