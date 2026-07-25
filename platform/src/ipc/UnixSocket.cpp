#include "elap/ipc/UnixSocket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <limits>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace elap::ipc {
namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

std::string systemError(const std::string& operation)
{
    return operation + " failed: " + std::strerror(errno);
}

bool writeAll(int fd, const void* data, std::size_t size, std::string* errorMessage)
{
    const auto* bytes = static_cast<const char*>(data);
    std::size_t written = 0;
    while (written < size) {
        const auto result = ::send(fd, bytes + written, size - written, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError(errorMessage, systemError("send"));
            return false;
        }
        if (result == 0) {
            setError(errorMessage, "send failed: connection closed");
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

bool readAll(int fd, void* data, std::size_t size, std::string* errorMessage)
{
    auto* bytes = static_cast<char*>(data);
    std::size_t read = 0;
    while (read < size) {
        const auto result = ::recv(fd, bytes + read, size - read, 0);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError(errorMessage, systemError("receive"));
            return false;
        }
        if (result == 0) {
            setError(errorMessage, "receive failed: connection closed");
            return false;
        }
        read += static_cast<std::size_t>(result);
    }
    return true;
}

bool fillAddress(const std::string& path, sockaddr_un& address, std::string* errorMessage)
{
    if (path.empty()) {
        setError(errorMessage, "unix socket path is empty");
        return false;
    }
    if (path.size() >= sizeof(address.sun_path)) {
        setError(errorMessage, "unix socket path is too long");
        return false;
    }

    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
    return true;
}

} // namespace

UnixSocketConnection::UnixSocketConnection(int fd)
    : fd_(fd)
{
}

UnixSocketConnection::~UnixSocketConnection()
{
    close();
}

UnixSocketConnection::UnixSocketConnection(UnixSocketConnection&& other) noexcept
    : fd_(other.fd_)
{
    other.fd_ = -1;
}

UnixSocketConnection& UnixSocketConnection::operator=(UnixSocketConnection&& other) noexcept
{
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool UnixSocketConnection::isOpen() const
{
    return fd_ >= 0;
}

int UnixSocketConnection::nativeHandle() const
{
    return fd_;
}

bool UnixSocketConnection::sendMessage(const std::string& message, std::string* errorMessage)
{
    if (!isOpen()) {
        setError(errorMessage, "send failed: connection is not open");
        return false;
    }
    if (message.size() > std::numeric_limits<std::uint32_t>::max()) {
        setError(errorMessage, "send failed: message is too large");
        return false;
    }

    const auto size = htonl(static_cast<std::uint32_t>(message.size()));
    return writeAll(fd_, &size, sizeof(size), errorMessage)
        && writeAll(fd_, message.data(), message.size(), errorMessage);
}

bool UnixSocketConnection::receiveMessage(std::string& message,
                                          std::size_t maxSize,
                                          std::string* errorMessage)
{
    message.clear();
    if (!isOpen()) {
        setError(errorMessage, "receive failed: connection is not open");
        return false;
    }

    std::uint32_t encodedSize = 0;
    if (!readAll(fd_, &encodedSize, sizeof(encodedSize), errorMessage)) {
        return false;
    }

    const auto size = static_cast<std::size_t>(ntohl(encodedSize));
    if (size > maxSize) {
        setError(errorMessage, "receive failed: message exceeds maximum size");
        return false;
    }

    message.resize(size);
    if (size == 0) {
        return true;
    }
    return readAll(fd_, message.data(), size, errorMessage);
}

void UnixSocketConnection::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

UnixSocketServer::~UnixSocketServer()
{
    close();
}

bool UnixSocketServer::listen(const std::string& path,
                              int backlog,
                              std::string* errorMessage)
{
    close();

    sockaddr_un address {};
    if (!fillAddress(path, address, errorMessage)) {
        return false;
    }

    const int socketFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0) {
        setError(errorMessage, systemError("socket"));
        return false;
    }

    ::unlink(path.c_str());
    if (::bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        setError(errorMessage, systemError("bind"));
        ::close(socketFd);
        return false;
    }

    if (::listen(socketFd, backlog) < 0) {
        setError(errorMessage, systemError("listen"));
        ::close(socketFd);
        ::unlink(path.c_str());
        return false;
    }

    fd_ = socketFd;
    path_ = path;
    return true;
}

UnixSocketConnection UnixSocketServer::accept(std::string* errorMessage)
{
    if (!isListening()) {
        setError(errorMessage, "accept failed: server is not listening");
        return UnixSocketConnection {};
    }

    while (true) {
        const int clientFd = ::accept(fd_, nullptr, nullptr);
        if (clientFd >= 0) {
            return UnixSocketConnection(clientFd);
        }
        if (errno == EINTR) {
            continue;
        }
        setError(errorMessage, systemError("accept"));
        return UnixSocketConnection {};
    }
}

bool UnixSocketServer::isListening() const
{
    return fd_ >= 0;
}

const std::string& UnixSocketServer::path() const
{
    return path_;
}

void UnixSocketServer::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (!path_.empty()) {
        ::unlink(path_.c_str());
        path_.clear();
    }
}

UnixSocketConnection UnixSocketClient::connect(const std::string& path,
                                               std::string* errorMessage)
{
    sockaddr_un address {};
    if (!fillAddress(path, address, errorMessage)) {
        return UnixSocketConnection {};
    }

    const int socketFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0) {
        setError(errorMessage, systemError("socket"));
        return UnixSocketConnection {};
    }

    if (::connect(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        setError(errorMessage, systemError("connect"));
        ::close(socketFd);
        return UnixSocketConnection {};
    }

    return UnixSocketConnection(socketFd);
}

} // namespace elap::ipc
