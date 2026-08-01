#pragma once

#include "elap/device/IDevice.hpp"

#include <cstddef>
#include <string>

namespace elap::device {

class FileDevice final : public IDevice {
public:
    FileDevice(std::string name, std::string path, int flags);
    ~FileDevice() override;

    FileDevice(const FileDevice&) = delete;
    FileDevice& operator=(const FileDevice&) = delete;

    FileDevice(FileDevice&& other) noexcept;
    FileDevice& operator=(FileDevice&& other) noexcept;

    const char* name() const override;
    bool open(std::string* errorMessage = nullptr) override;
    void close() override;
    DeviceState state() const override;

    const std::string& path() const;
    int nativeHandle() const;

    bool write(const void* data, std::size_t size, std::string* errorMessage = nullptr);
    bool read(void* data, std::size_t size, std::size_t& bytesRead,
              std::string* errorMessage = nullptr);

private:
    std::string name_;
    std::string path_;
    int flags_;
    int fd_ {-1};
    DeviceState state_ {DeviceState::Closed};
};

} // namespace elap::device
