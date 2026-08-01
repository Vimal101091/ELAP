#include "elap/ipc/EventBus.hpp"
#include "elap/ipc/PosixMessageQueue.hpp"
#include "elap/ipc/SharedMemory.hpp"
#include "elap/ipc/UnixSocket.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

std::string testSocketPath(const char* name)
{
    return "/tmp/elap_" + std::string(name) + "_" + std::to_string(::getpid()) + ".sock";
}

std::string testObjectName(const char* name)
{
    return "/elap_" + std::string(name) + "_" + std::to_string(::getpid());
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

    const auto regularFilePath = testSocketPath("ipc_regular_file");
    {
        std::ofstream file(regularFilePath);
        file << "must-not-delete";
    }

    elap::ipc::UnixSocketServer invalidServer;
    assert(!invalidServer.listen(regularFilePath, 1, &error));
    assert(!error.empty());

    std::ifstream file(regularFilePath);
    std::string contents;
    file >> contents;
    assert(contents == "must-not-delete");
    ::unlink(regularFilePath.c_str());
}

void runUnixSocketTimeoutTest()
{
    const auto path = testSocketPath("ipc_timeout");
    std::string error;

    elap::ipc::UnixSocketServer server;
    assert(server.listen(path, 1, &error));

    bool receiveTimedOut = false;
    std::thread serverThread([&server, &receiveTimedOut]() {
        std::string localError;
        auto connection = server.accept(&localError);
        assert(connection.isOpen());

        std::string message;
        receiveTimedOut = !connection.receiveMessage(message,
                                                     1024,
                                                     std::chrono::milliseconds(25),
                                                     &localError);
        assert(!localError.empty());
    });

    auto client = elap::ipc::UnixSocketClient::connect(path, &error);
    assert(client.isOpen());

    serverThread.join();
    assert(receiveTimedOut);

    client.close();
    server.close();
}

void runMessageQueueRoundTripTest()
{
    const auto name = testObjectName("mq_round_trip");
    std::string error;

    elap::ipc::PosixMessageQueue producer;
    assert(producer.create(name, 4, 128, &error));
    assert(producer.isOpen());
    assert(producer.name() == name);
    assert(producer.maxMessageSize() >= 128);

    elap::ipc::PosixMessageQueue consumer;
    assert(consumer.open(name, &error));
    assert(consumer.isOpen());

    assert(producer.sendMessage("command:start", 7, &error));

    std::string message;
    unsigned int priority = 0;
    assert(consumer.receiveMessage(message, &priority, &error));
    assert(message == "command:start");
    assert(priority == 7);

    assert(!producer.sendMessage(std::string(256, 'x'), 0, &error));
    assert(!error.empty());

    elap::ipc::PosixMessageQueue duplicate;
    assert(!duplicate.create(name, 4, 128, &error));
    assert(!error.empty());

    elap::ipc::PosixMessageQueue emptyQueue;
    const auto timeoutName = testObjectName("mq_timeout");
    assert(emptyQueue.create(timeoutName, 1, 64, &error));
    assert(!emptyQueue.receiveMessage(message,
                                      &priority,
                                      std::chrono::milliseconds(10),
                                      &error));
    assert(!error.empty());
    assert(emptyQueue.sendMessage("first", 0, &error));
    assert(!emptyQueue.sendMessage("second",
                                   0,
                                   std::chrono::milliseconds(10),
                                   &error));
    assert(!error.empty());
    emptyQueue.close();

    consumer.close();
    producer.close();
}

void runSharedMemoryRoundTripTest()
{
    const auto name = testObjectName("shm_round_trip");
    std::string error;

    elap::ipc::SharedMemoryRegion writer;
    assert(writer.create(name, 256, &error));
    assert(writer.isMapped());
    assert(writer.name() == name);
    assert(writer.size() == 256);

    elap::ipc::SharedMemoryRegion reader;
    assert(reader.open(name, 256, &error));
    assert(reader.isMapped());

    elap::ipc::SharedMemoryRegion duplicate;
    assert(!duplicate.create(name, 256, &error));
    assert(!error.empty());

    elap::ipc::SharedMemoryRegion oversizedReader;
    assert(!oversizedReader.open(name, 512, &error));
    assert(!error.empty());

    const std::string payload = "shared-memory-payload";
    assert(writer.write(16, payload.data(), payload.size(), &error));

    char buffer[64] {};
    assert(reader.read(16, buffer, payload.size(), &error));
    assert(std::string(buffer, payload.size()) == payload);

    assert(!reader.read(250, buffer, sizeof(buffer), &error));
    assert(!error.empty());

    reader.close();
    writer.close();
}

void runEventBusRoundTripTest()
{
    const auto name = testObjectName("event_bus");
    std::string error;

    elap::ipc::MessageQueueEventBus publisher;
    assert(publisher.create(name, 4, 256, &error));
    assert(publisher.isOpen());

    elap::ipc::MessageQueueEventBus subscriber;
    assert(subscriber.open(name, &error));
    assert(subscriber.isOpen());

    assert(publisher.publish({"service.health", "state=running"}, &error));

    elap::ipc::Event event;
    assert(subscriber.receive(event, &error));
    assert(event.topic == "service.health");
    assert(event.payload == "state=running");

    assert(!publisher.publish({"bad\ntopic", "payload"}, &error));
    assert(!error.empty());

    elap::ipc::Event timeoutEvent;
    assert(!subscriber.receive(timeoutEvent, std::chrono::milliseconds(10), &error));
    assert(!error.empty());

    subscriber.close();
    publisher.close();
}

void runIpcTests()
{
    runUnixSocketRoundTripTest();
    runUnixSocketValidationTest();
    runUnixSocketTimeoutTest();
    runMessageQueueRoundTripTest();
    runSharedMemoryRoundTripTest();
    runEventBusRoundTripTest();
}

} // namespace

struct IpcTestRunner {
    IpcTestRunner()
    {
        runIpcTests();
    }
} ipcTestRunner;
