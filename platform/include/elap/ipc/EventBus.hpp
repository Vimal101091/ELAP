#pragma once

#include "elap/ipc/PosixMessageQueue.hpp"

#include <chrono>
#include <string>

namespace elap::ipc {

struct Event {
    std::string topic;
    std::string payload;
};

class MessageQueueEventBus {
public:
    bool create(const std::string& name,
                long maxEvents = 16,
                long maxEventSize = 2048,
                std::string* errorMessage = nullptr);
    bool open(const std::string& name, std::string* errorMessage = nullptr);

    bool publish(const Event& event, std::string* errorMessage = nullptr);
    bool publish(const Event& event,
                 std::chrono::milliseconds timeout,
                 std::string* errorMessage = nullptr);
    bool receive(Event& event, std::string* errorMessage = nullptr);
    bool receive(Event& event,
                 std::chrono::milliseconds timeout,
                 std::string* errorMessage = nullptr);

    bool isOpen() const;
    void close();
    void unlink();

private:
    PosixMessageQueue queue_;
};

} // namespace elap::ipc
