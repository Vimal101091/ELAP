#include "elap/ipc/SharedMemory.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

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

bool validSharedMemoryName(const std::string& name, std::string* errorMessage)
{
    if (name.empty() || name.front() != '/') {
        setError(errorMessage, "shared memory name must start with /");
        return false;
    }
    if (name.find('/', 1) != std::string::npos) {
        setError(errorMessage, "shared memory name must not contain additional / characters");
        return false;
    }
    return true;
}

bool inRange(std::size_t offset, std::size_t count, std::size_t size)
{
    return offset <= size && count <= size - offset;
}

bool canRepresentAsOffT(std::size_t size)
{
    return size <= static_cast<std::size_t>(std::numeric_limits<off_t>::max());
}

bool loadSharedMemorySize(int fd, std::size_t& size, std::string* errorMessage)
{
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        setError(errorMessage, systemError("fstat"));
        return false;
    }
    if (status.st_size <= 0) {
        setError(errorMessage, "shared memory open failed: backing object has invalid size");
        return false;
    }

    size = static_cast<std::size_t>(status.st_size);
    return true;
}

} // namespace

SharedMemoryRegion::~SharedMemoryRegion()
{
    close();
}

SharedMemoryRegion::SharedMemoryRegion(SharedMemoryRegion&& other) noexcept
    : fd_(other.fd_)
    , name_(std::move(other.name_))
    , mapping_(other.mapping_)
    , size_(other.size_)
    , owner_(other.owner_)
{
    other.fd_ = -1;
    other.mapping_ = nullptr;
    other.size_ = 0;
    other.owner_ = false;
}

SharedMemoryRegion& SharedMemoryRegion::operator=(SharedMemoryRegion&& other) noexcept
{
    if (this != &other) {
        close();
        fd_ = other.fd_;
        name_ = std::move(other.name_);
        mapping_ = other.mapping_;
        size_ = other.size_;
        owner_ = other.owner_;

        other.fd_ = -1;
        other.mapping_ = nullptr;
        other.size_ = 0;
        other.owner_ = false;
    }
    return *this;
}

bool SharedMemoryRegion::create(const std::string& name,
                                std::size_t size,
                                std::string* errorMessage)
{
    close();
    if (!validSharedMemoryName(name, errorMessage)) {
        return false;
    }
    if (size == 0) {
        setError(errorMessage, "shared memory size must be positive");
        return false;
    }
    if (!canRepresentAsOffT(size)) {
        setError(errorMessage, "shared memory size is too large");
        return false;
    }

    const int fd = ::shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        setError(errorMessage, systemError("shm_open"));
        return false;
    }

    if (::ftruncate(fd, static_cast<off_t>(size)) != 0) {
        setError(errorMessage, systemError("ftruncate"));
        ::close(fd);
        ::shm_unlink(name.c_str());
        return false;
    }

    void* mapping = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        setError(errorMessage, systemError("mmap"));
        ::close(fd);
        ::shm_unlink(name.c_str());
        return false;
    }

    fd_ = fd;
    name_ = name;
    mapping_ = mapping;
    size_ = size;
    owner_ = true;
    return true;
}

bool SharedMemoryRegion::open(const std::string& name,
                              std::size_t size,
                              std::string* errorMessage)
{
    close();
    if (!validSharedMemoryName(name, errorMessage)) {
        return false;
    }
    if (size == 0) {
        setError(errorMessage, "shared memory size must be positive");
        return false;
    }

    const int fd = ::shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        setError(errorMessage, systemError("shm_open"));
        return false;
    }

    std::size_t backingSize = 0;
    if (!loadSharedMemorySize(fd, backingSize, errorMessage)) {
        ::close(fd);
        return false;
    }
    if (size > backingSize) {
        setError(errorMessage, "shared memory open failed: requested size exceeds backing object size");
        ::close(fd);
        return false;
    }

    void* mapping = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        setError(errorMessage, systemError("mmap"));
        ::close(fd);
        return false;
    }

    fd_ = fd;
    name_ = name;
    mapping_ = mapping;
    size_ = size;
    owner_ = false;
    return true;
}

bool SharedMemoryRegion::isMapped() const
{
    return mapping_ != nullptr;
}

const std::string& SharedMemoryRegion::name() const
{
    return name_;
}

std::size_t SharedMemoryRegion::size() const
{
    return size_;
}

void* SharedMemoryRegion::data()
{
    return mapping_;
}

const void* SharedMemoryRegion::data() const
{
    return mapping_;
}

bool SharedMemoryRegion::write(std::size_t offset,
                               const void* source,
                               std::size_t size,
                               std::string* errorMessage)
{
    if (!isMapped()) {
        setError(errorMessage, "shared memory write failed: region is not mapped");
        return false;
    }
    if (source == nullptr && size > 0) {
        setError(errorMessage, "shared memory write failed: source is null");
        return false;
    }
    if (!inRange(offset, size, size_)) {
        setError(errorMessage, "shared memory write failed: range is out of bounds");
        return false;
    }

    std::memcpy(static_cast<char*>(mapping_) + offset, source, size);
    return true;
}

bool SharedMemoryRegion::read(std::size_t offset,
                              void* destination,
                              std::size_t size,
                              std::string* errorMessage) const
{
    if (!isMapped()) {
        setError(errorMessage, "shared memory read failed: region is not mapped");
        return false;
    }
    if (destination == nullptr && size > 0) {
        setError(errorMessage, "shared memory read failed: destination is null");
        return false;
    }
    if (!inRange(offset, size, size_)) {
        setError(errorMessage, "shared memory read failed: range is out of bounds");
        return false;
    }

    std::memcpy(destination, static_cast<const char*>(mapping_) + offset, size);
    return true;
}

void SharedMemoryRegion::close()
{
    if (mapping_ != nullptr) {
        ::munmap(mapping_, size_);
        mapping_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (owner_ && !name_.empty()) {
        ::shm_unlink(name_.c_str());
    }
    name_.clear();
    size_ = 0;
    owner_ = false;
}

void SharedMemoryRegion::unlink()
{
    if (!name_.empty()) {
        ::shm_unlink(name_.c_str());
    }
    owner_ = false;
}

} // namespace elap::ipc
