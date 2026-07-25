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
    const char* message = argumentValue(argc, argv, "--message");

    if (queueName == nullptr || message == nullptr) {
        std::cerr << "usage: " << argv[0] << " --queue <name> --message <text>\n";
        return 2;
    }

    std::string error;
    elap::ipc::PosixMessageQueue queue;
    if (!queue.open(queueName, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    if (!queue.sendMessage(message, 0, &error)) {
        std::cerr << error << '\n';
        return 1;
    }

    return 0;
}
