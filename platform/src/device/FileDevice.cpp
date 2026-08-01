#include "elap/device/FileDevice.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace elap::device {
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

} // namespace

FileDevice::FileDevice(std::string name, std::string path, int flags)
    : name_(std::move(name))
    , path_(std::move(path))
    , flags_(flags)
{
}

FileDevice::~FileDevice()
{
    close();
}

FileDevice::FileDevice(FileDevice&& other) noexcept
    : name_(std::move(other.name_))
    , path_(std::move(other.path_))
    , flags_(other.flags_)
    , fd_(other.fd_)
    , state_(other.state_)
{
    other.fd_ = -1;
    other.state_ = DeviceState::Closed;
}

FileDevice& FileDevice::operator=(FileDevice&& other) noexcept
{
    if (this != &other) {
        close();
        name_ = std::move(other.name_);
        path_ = std::move(other.path_);
        flags_ = other.flags_;
        fd_ = other.fd_;
        state_ = other.state_;
        other.fd_ = -1;
        other.state_ = DeviceState::Closed;
    }
    return *this;
}

const char* FileDevice::name() const
{
    return name_.c_str();
}

bool FileDevice::open(std::string* errorMessage)
{
    close();
    fd_ = ::open(path_.c_str(), flags_);
    if (fd_ < 0) {
        state_ = DeviceState::Failed;
        setError(errorMessage, systemError("open " + path_));
        return false;
    }
    state_ = DeviceState::Open;
    return true;
}

void FileDevice::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (state_ != DeviceState::Failed) {
        state_ = DeviceState::Closed;
    }
}

DeviceState FileDevice::state() const
{
    return state_;
}

const std::string& FileDevice::path() const
{
    return path_;
}

int FileDevice::nativeHandle() const
{
    return fd_;
}

bool FileDevice::write(const void* data, std::size_t size, std::string* errorMessage)
{
    if (fd_ < 0) {
        setError(errorMessage, "write failed: device is not open");
        return false;
    }

    const auto* bytes = static_cast<const char*>(data);
    std::size_t written = 0;
    while (written < size) {
        const auto result = ::write(fd_, bytes + written, size - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError(errorMessage, systemError("write"));
            return false;
        }
        if (result == 0) {
            setError(errorMessage, "write failed: no bytes written");
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

bool FileDevice::read(void* data, std::size_t size, std::size_t& bytesRead,
                      std::string* errorMessage)
{
    bytesRead = 0;
    if (fd_ < 0) {
        setError(errorMessage, "read failed: device is not open");
        return false;
    }

    while (true) {
        const auto result = ::read(fd_, data, size);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            setError(errorMessage, systemError("read"));
            return false;
        }
        bytesRead = static_cast<std::size_t>(result);
        return true;
    }
}

} // namespace elap::device
