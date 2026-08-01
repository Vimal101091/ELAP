#include "elap/ipc/UnixSocket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

int pollTimeout(std::chrono::milliseconds timeout)
{
    if (timeout.count() > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (timeout.count() < 0) {
        return -1;
    }
    return static_cast<int>(timeout.count());
}

bool waitForDescriptor(int fd,
                       short events,
                       std::chrono::steady_clock::time_point deadline,
                       const std::string& operation,
                       std::string* errorMessage)
{
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = now >= deadline
            ? std::chrono::milliseconds(0)
            : std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        pollfd descriptor {};
        descriptor.fd = fd;
        descriptor.events = events;

        const auto result = ::poll(&descriptor, 1, pollTimeout(remaining));
        if (result > 0) {
            if ((descriptor.revents & events) != 0) {
                return true;
            }
            if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                setError(errorMessage, operation + " failed: descriptor is not usable");
                return false;
            }
        } else if (result == 0) {
            setError(errorMessage, operation + " timed out");
            return false;
        } else if (errno != EINTR) {
            setError(errorMessage, systemError("poll"));
            return false;
        }
    }
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

bool writeAll(int fd,
              const void* data,
              std::size_t size,
              std::chrono::steady_clock::time_point deadline,
              std::string* errorMessage)
{
    const auto* bytes = static_cast<const char*>(data);
    std::size_t written = 0;
    while (written < size) {
        if (!waitForDescriptor(fd, POLLOUT, deadline, "send", errorMessage)) {
            return false;
        }

        const auto result = ::send(fd, bytes + written, size - written, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
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

bool readAll(int fd,
             void* data,
             std::size_t size,
             std::chrono::steady_clock::time_point deadline,
             std::string* errorMessage)
{
    auto* bytes = static_cast<char*>(data);
    std::size_t read = 0;
    while (read < size) {
        if (!waitForDescriptor(fd, POLLIN, deadline, "receive", errorMessage)) {
            return false;
        }

        const auto result = ::recv(fd, bytes + read, size - read, 0);
        if (result < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
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

bool socketHasActiveListener(const sockaddr_un& address)
{
    const int probeFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (probeFd < 0) {
        return true;
    }

    const bool connected = (::connect(probeFd,
                                      reinterpret_cast<const sockaddr*>(&address),
                                      sizeof(address)) == 0);
    ::close(probeFd);
    return connected;
}

bool removeStaleSocketPath(const std::string& path,
                           const sockaddr_un& address,
                           std::string* errorMessage)
{
    struct stat pathStatus {};
    if (::lstat(path.c_str(), &pathStatus) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        setError(errorMessage, systemError("lstat"));
        return false;
    }

    if (!S_ISSOCK(pathStatus.st_mode)) {
        setError(errorMessage, "bind failed: path exists and is not a unix socket");
        return false;
    }

    if (socketHasActiveListener(address)) {
        setError(errorMessage, "bind failed: unix socket path already has an active listener");
        return false;
    }

    if (::unlink(path.c_str()) != 0 && errno != ENOENT) {
        setError(errorMessage, systemError("unlink"));
        return false;
    }
    return true;
}

bool loadSocketPathIdentity(const std::string& path,
                            dev_t& device,
                            ino_t& inode,
                            std::string* errorMessage)
{
    struct stat pathStatus {};
    if (::lstat(path.c_str(), &pathStatus) != 0) {
        setError(errorMessage, systemError("lstat"));
        return false;
    }
    if (!S_ISSOCK(pathStatus.st_mode)) {
        setError(errorMessage, "bind failed: bound path is not a unix socket");
        return false;
    }

    device = pathStatus.st_dev;
    inode = pathStatus.st_ino;
    return true;
}

void unlinkOwnedSocketPath(const std::string& path, dev_t device, ino_t inode)
{
    if (path.empty()) {
        return;
    }

    struct stat pathStatus {};
    if (::lstat(path.c_str(), &pathStatus) != 0) {
        return;
    }
    if (S_ISSOCK(pathStatus.st_mode)
        && pathStatus.st_dev == device
        && pathStatus.st_ino == inode) {
        ::unlink(path.c_str());
    }
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

bool UnixSocketConnection::sendMessage(const std::string& message,
                                       std::chrono::milliseconds timeout,
                                       std::string* errorMessage)
{
    if (!isOpen()) {
        setError(errorMessage, "send failed: connection is not open");
        return false;
    }
    if (timeout.count() < 0) {
        setError(errorMessage, "send failed: timeout must not be negative");
        return false;
    }
    if (message.size() > std::numeric_limits<std::uint32_t>::max()) {
        setError(errorMessage, "send failed: message is too large");
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    const auto size = htonl(static_cast<std::uint32_t>(message.size()));
    return writeAll(fd_, &size, sizeof(size), deadline, errorMessage)
        && writeAll(fd_, message.data(), message.size(), deadline, errorMessage);
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

bool UnixSocketConnection::receiveMessage(std::string& message,
                                          std::size_t maxSize,
                                          std::chrono::milliseconds timeout,
                                          std::string* errorMessage)
{
    message.clear();
    if (!isOpen()) {
        setError(errorMessage, "receive failed: connection is not open");
        return false;
    }
    if (timeout.count() < 0) {
        setError(errorMessage, "receive failed: timeout must not be negative");
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::uint32_t encodedSize = 0;
    if (!readAll(fd_, &encodedSize, sizeof(encodedSize), deadline, errorMessage)) {
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
    return readAll(fd_, message.data(), size, deadline, errorMessage);
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

    if (!removeStaleSocketPath(path, address, errorMessage)) {
        ::close(socketFd);
        return false;
    }

    if (::bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        setError(errorMessage, systemError("bind"));
        ::close(socketFd);
        return false;
    }

    dev_t pathDevice {};
    ino_t pathInode {};
    if (!loadSocketPathIdentity(path, pathDevice, pathInode, errorMessage)) {
        ::close(socketFd);
        ::unlink(path.c_str());
        return false;
    }

    if (::listen(socketFd, backlog) < 0) {
        setError(errorMessage, systemError("listen"));
        ::close(socketFd);
        unlinkOwnedSocketPath(path, pathDevice, pathInode);
        return false;
    }

    fd_ = socketFd;
    path_ = path;
    pathDevice_ = pathDevice;
    pathInode_ = pathInode;
    ownsPath_ = true;
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

bool UnixSocketServer::tryAccept(UnixSocketConnection& connection,
                                 std::chrono::milliseconds timeout,
                                 std::string* errorMessage)
{
    connection.close();
    if (!isListening()) {
        setError(errorMessage, "accept failed: server is not listening");
        return false;
    }

    pollfd descriptor {};
    descriptor.fd = fd_;
    descriptor.events = POLLIN;

    while (true) {
        const auto result = ::poll(&descriptor, 1, static_cast<int>(timeout.count()));
        if (result > 0) {
            connection = accept(errorMessage);
            return connection.isOpen();
        }
        if (result == 0) {
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        setError(errorMessage, systemError("poll"));
        return false;
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
    if (ownsPath_) {
        unlinkOwnedSocketPath(path_, pathDevice_, pathInode_);
    }
    path_.clear();
    pathDevice_ = {};
    pathInode_ = {};
    ownsPath_ = false;
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
