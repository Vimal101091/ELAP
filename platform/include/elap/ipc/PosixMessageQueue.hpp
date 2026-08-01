#pragma once

#include <cstddef>
#include <chrono>
#include <string>

namespace elap::ipc {

class PosixMessageQueue {
public:
    PosixMessageQueue() = default;
    ~PosixMessageQueue();

    PosixMessageQueue(const PosixMessageQueue&) = delete;
    PosixMessageQueue& operator=(const PosixMessageQueue&) = delete;

    PosixMessageQueue(PosixMessageQueue&& other) noexcept;
    PosixMessageQueue& operator=(PosixMessageQueue&& other) noexcept;

    bool create(const std::string& name,
                long maxMessages = 8,
                long maxMessageSize = 1024,
                std::string* errorMessage = nullptr);
    bool open(const std::string& name, std::string* errorMessage = nullptr);

    bool isOpen() const;
    const std::string& name() const;
    std::size_t maxMessageSize() const;

    bool sendMessage(const std::string& message,
                     unsigned int priority = 0,
                     std::string* errorMessage = nullptr);
    bool sendMessage(const std::string& message,
                     unsigned int priority,
                     std::chrono::milliseconds timeout,
                     std::string* errorMessage = nullptr);
    bool receiveMessage(std::string& message,
                        unsigned int* priority = nullptr,
                        std::string* errorMessage = nullptr);
    bool receiveMessage(std::string& message,
                        unsigned int* priority,
                        std::chrono::milliseconds timeout,
                        std::string* errorMessage = nullptr);

    void close();
    void unlink();

private:
    using Descriptor = int;

    Descriptor descriptor_ {-1};
    std::string name_;
    std::size_t maxMessageSize_ {0};
    bool owner_ {false};
};

} // namespace elap::ipc
