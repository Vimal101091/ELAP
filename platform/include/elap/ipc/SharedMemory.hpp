#pragma once

#include <cstddef>
#include <string>

namespace elap::ipc {

class SharedMemoryRegion {
public:
    SharedMemoryRegion() = default;
    ~SharedMemoryRegion();

    SharedMemoryRegion(const SharedMemoryRegion&) = delete;
    SharedMemoryRegion& operator=(const SharedMemoryRegion&) = delete;

    SharedMemoryRegion(SharedMemoryRegion&& other) noexcept;
    SharedMemoryRegion& operator=(SharedMemoryRegion&& other) noexcept;

    bool create(const std::string& name, std::size_t size, std::string* errorMessage = nullptr);
    bool open(const std::string& name, std::size_t size, std::string* errorMessage = nullptr);

    bool isMapped() const;
    const std::string& name() const;
    std::size_t size() const;
    void* data();
    const void* data() const;

    bool write(std::size_t offset,
               const void* source,
               std::size_t size,
               std::string* errorMessage = nullptr);
    bool read(std::size_t offset,
              void* destination,
              std::size_t size,
              std::string* errorMessage = nullptr) const;

    void close();
    void unlink();

private:
    int fd_ {-1};
    std::string name_;
    void* mapping_ {nullptr};
    std::size_t size_ {0};
    bool owner_ {false};
};

} // namespace elap::ipc
