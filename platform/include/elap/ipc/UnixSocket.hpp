#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace elap::ipc {

class UnixSocketConnection {
public:
    UnixSocketConnection() = default;
    explicit UnixSocketConnection(int fd);
    ~UnixSocketConnection();

    UnixSocketConnection(const UnixSocketConnection&) = delete;
    UnixSocketConnection& operator=(const UnixSocketConnection&) = delete;

    UnixSocketConnection(UnixSocketConnection&& other) noexcept;
    UnixSocketConnection& operator=(UnixSocketConnection&& other) noexcept;

    bool isOpen() const;
    int nativeHandle() const;

    bool sendMessage(const std::string& message, std::string* errorMessage = nullptr);
    bool receiveMessage(std::string& message,
                        std::size_t maxSize = 1024 * 1024,
                        std::string* errorMessage = nullptr);
    void close();

private:
    int fd_ {-1};
};

class UnixSocketServer {
public:
    UnixSocketServer() = default;
    ~UnixSocketServer();

    UnixSocketServer(const UnixSocketServer&) = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    bool listen(const std::string& path, int backlog = 8, std::string* errorMessage = nullptr);
    UnixSocketConnection accept(std::string* errorMessage = nullptr);
    bool isListening() const;
    const std::string& path() const;
    void close();

private:
    int fd_ {-1};
    std::string path_;
};

class UnixSocketClient {
public:
    static UnixSocketConnection connect(const std::string& path,
                                        std::string* errorMessage = nullptr);
};

} // namespace elap::ipc
