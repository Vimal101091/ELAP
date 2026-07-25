#include "elap/ipc/UnixSocket.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::string testSocketPath(const char* name)
{
    return "/tmp/elap_" + std::string(name) + "_" + std::to_string(::getpid()) + ".sock";
}

void runUnixSocketRoundTripTest()
{
    const auto path = testSocketPath("ipc_round_trip");
    std::string error;

    elap::ipc::UnixSocketServer server;
    assert(server.listen(path, 1, &error));
    assert(server.isListening());
    assert(server.path() == path);

    std::string receivedByServer;
    std::thread serverThread([&server, &receivedByServer]() {
        std::string localError;
        auto connection = server.accept(&localError);
        assert(connection.isOpen());
        assert(connection.receiveMessage(receivedByServer, 1024, &localError));
        assert(connection.sendMessage("pong", &localError));
    });

    auto client = elap::ipc::UnixSocketClient::connect(path, &error);
    assert(client.isOpen());
    assert(client.sendMessage("ping", &error));

    std::string response;
    assert(client.receiveMessage(response, 1024, &error));
    assert(response == "pong");

    serverThread.join();
    assert(receivedByServer == "ping");

    client.close();
    server.close();
}

void runUnixSocketValidationTest()
{
    std::string error;
    const auto missingPath = testSocketPath("ipc_missing");
    auto missingClient = elap::ipc::UnixSocketClient::connect(missingPath, &error);
    assert(!missingClient.isOpen());
    assert(!error.empty());

    elap::ipc::UnixSocketConnection closedConnection;
    assert(!closedConnection.sendMessage("data", &error));
    assert(!error.empty());

    const auto path = testSocketPath("ipc_limit");
    elap::ipc::UnixSocketServer server;
    assert(server.listen(path, 1, &error));

    std::thread serverThread([&server]() {
        std::string localError;
        auto connection = server.accept(&localError);
        assert(connection.isOpen());
        assert(connection.sendMessage("oversized", &localError));
    });

    auto client = elap::ipc::UnixSocketClient::connect(path, &error);
    assert(client.isOpen());

    std::string message;
    assert(!client.receiveMessage(message, 4, &error));
    assert(!error.empty());

    serverThread.join();
    server.close();
}

void runIpcTests()
{
    runUnixSocketRoundTripTest();
    runUnixSocketValidationTest();
}

} // namespace

struct IpcTestRunner {
    IpcTestRunner()
    {
        runIpcTests();
    }
} ipcTestRunner;
