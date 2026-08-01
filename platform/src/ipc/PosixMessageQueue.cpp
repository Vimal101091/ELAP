#include "elap/ipc/PosixMessageQueue.hpp"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <utility>
#include <vector>

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

bool validQueueName(const std::string& name, std::string* errorMessage)
{
    if (name.empty() || name.front() != '/') {
        setError(errorMessage, "message queue name must start with /");
        return false;
    }
    if (name.find('/', 1) != std::string::npos) {
        setError(errorMessage, "message queue name must not contain additional / characters");
        return false;
    }
    return true;
}

bool loadAttributes(mqd_t descriptor, std::size_t& maxMessageSize, std::string* errorMessage)
{
    mq_attr attributes {};
    if (::mq_getattr(descriptor, &attributes) != 0) {
        setError(errorMessage, systemError("mq_getattr"));
        return false;
    }
    maxMessageSize = static_cast<std::size_t>(attributes.mq_msgsize);
    return true;
}

bool loadDeadline(std::chrono::milliseconds timeout,
                  timespec& deadline,
                  std::string* errorMessage)
{
    if (timeout.count() < 0) {
        setError(errorMessage, "message queue timeout must not be negative");
        return false;
    }
    if (::clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
        setError(errorMessage, systemError("clock_gettime"));
        return false;
    }

    constexpr long nanosecondsPerSecond = 1000L * 1000L * 1000L;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        timeout - seconds);

    deadline.tv_sec += static_cast<time_t>(seconds.count());
    deadline.tv_nsec += static_cast<long>(milliseconds.count()) * 1000L * 1000L;
    if (deadline.tv_nsec >= nanosecondsPerSecond) {
        deadline.tv_sec += deadline.tv_nsec / nanosecondsPerSecond;
        deadline.tv_nsec %= nanosecondsPerSecond;
    }
    return true;
}

} // namespace

PosixMessageQueue::~PosixMessageQueue()
{
    close();
}

PosixMessageQueue::PosixMessageQueue(PosixMessageQueue&& other) noexcept
    : descriptor_(other.descriptor_)
    , name_(std::move(other.name_))
    , maxMessageSize_(other.maxMessageSize_)
    , owner_(other.owner_)
{
    other.descriptor_ = -1;
    other.maxMessageSize_ = 0;
    other.owner_ = false;
}

PosixMessageQueue& PosixMessageQueue::operator=(PosixMessageQueue&& other) noexcept
{
    if (this != &other) {
        close();
        descriptor_ = other.descriptor_;
        name_ = std::move(other.name_);
        maxMessageSize_ = other.maxMessageSize_;
        owner_ = other.owner_;

        other.descriptor_ = -1;
        other.maxMessageSize_ = 0;
        other.owner_ = false;
    }
    return *this;
}

bool PosixMessageQueue::create(const std::string& name,
                               long maxMessages,
                               long maxMessageSize,
                               std::string* errorMessage)
{
    close();
    if (!validQueueName(name, errorMessage)) {
        return false;
    }
    if (maxMessages <= 0 || maxMessageSize <= 0) {
        setError(errorMessage, "message queue sizes must be positive");
        return false;
    }

    mq_attr attributes {};
    attributes.mq_maxmsg = maxMessages;
    attributes.mq_msgsize = maxMessageSize;

    const auto descriptor = ::mq_open(name.c_str(),
                                      O_CREAT | O_EXCL | O_RDWR,
                                      S_IRUSR | S_IWUSR,
                                      &attributes);
    if (descriptor == static_cast<mqd_t>(-1)) {
        setError(errorMessage, systemError("mq_open"));
        return false;
    }

    descriptor_ = static_cast<Descriptor>(descriptor);
    name_ = name;
    owner_ = true;
    return loadAttributes(descriptor, maxMessageSize_, errorMessage);
}

bool PosixMessageQueue::open(const std::string& name, std::string* errorMessage)
{
    close();
    if (!validQueueName(name, errorMessage)) {
        return false;
    }

    const auto descriptor = ::mq_open(name.c_str(), O_RDWR);
    if (descriptor == static_cast<mqd_t>(-1)) {
        setError(errorMessage, systemError("mq_open"));
        return false;
    }

    descriptor_ = static_cast<Descriptor>(descriptor);
    name_ = name;
    owner_ = false;
    return loadAttributes(descriptor, maxMessageSize_, errorMessage);
}

bool PosixMessageQueue::isOpen() const
{
    return descriptor_ != -1;
}

const std::string& PosixMessageQueue::name() const
{
    return name_;
}

std::size_t PosixMessageQueue::maxMessageSize() const
{
    return maxMessageSize_;
}

bool PosixMessageQueue::sendMessage(const std::string& message,
                                    unsigned int priority,
                                    std::string* errorMessage)
{
    if (!isOpen()) {
        setError(errorMessage, "mq_send failed: queue is not open");
        return false;
    }
    if (message.size() > maxMessageSize_) {
        setError(errorMessage, "mq_send failed: message exceeds maximum size");
        return false;
    }
    if (::mq_send(static_cast<mqd_t>(descriptor_),
                  message.data(),
                  message.size(),
                  priority) != 0) {
        setError(errorMessage, systemError("mq_send"));
        return false;
    }
    return true;
}

bool PosixMessageQueue::sendMessage(const std::string& message,
                                    unsigned int priority,
                                    std::chrono::milliseconds timeout,
                                    std::string* errorMessage)
{
    if (!isOpen()) {
        setError(errorMessage, "mq_timedsend failed: queue is not open");
        return false;
    }
    if (message.size() > maxMessageSize_) {
        setError(errorMessage, "mq_timedsend failed: message exceeds maximum size");
        return false;
    }

    timespec deadline {};
    if (!loadDeadline(timeout, deadline, errorMessage)) {
        return false;
    }

    if (::mq_timedsend(static_cast<mqd_t>(descriptor_),
                       message.data(),
                       message.size(),
                       priority,
                       &deadline) != 0) {
        setError(errorMessage, systemError("mq_timedsend"));
        return false;
    }
    return true;
}

bool PosixMessageQueue::receiveMessage(std::string& message,
                                       unsigned int* priority,
                                       std::string* errorMessage)
{
    message.clear();
    if (!isOpen()) {
        setError(errorMessage, "mq_receive failed: queue is not open");
        return false;
    }

    std::vector<char> buffer(maxMessageSize_);
    unsigned int receivedPriority = 0;
    const auto bytes = ::mq_receive(static_cast<mqd_t>(descriptor_),
                                    buffer.data(),
                                    buffer.size(),
                                    &receivedPriority);
    if (bytes < 0) {
        setError(errorMessage, systemError("mq_receive"));
        return false;
    }

    message.assign(buffer.data(), static_cast<std::size_t>(bytes));
    if (priority != nullptr) {
        *priority = receivedPriority;
    }
    return true;
}

bool PosixMessageQueue::receiveMessage(std::string& message,
                                       unsigned int* priority,
                                       std::chrono::milliseconds timeout,
                                       std::string* errorMessage)
{
    message.clear();
    if (!isOpen()) {
        setError(errorMessage, "mq_timedreceive failed: queue is not open");
        return false;
    }

    timespec deadline {};
    if (!loadDeadline(timeout, deadline, errorMessage)) {
        return false;
    }

    std::vector<char> buffer(maxMessageSize_);
    unsigned int receivedPriority = 0;
    const auto bytes = ::mq_timedreceive(static_cast<mqd_t>(descriptor_),
                                         buffer.data(),
                                         buffer.size(),
                                         &receivedPriority,
                                         &deadline);
    if (bytes < 0) {
        setError(errorMessage, systemError("mq_timedreceive"));
        return false;
    }

    message.assign(buffer.data(), static_cast<std::size_t>(bytes));
    if (priority != nullptr) {
        *priority = receivedPriority;
    }
    return true;
}

void PosixMessageQueue::close()
{
    if (descriptor_ != -1) {
        ::mq_close(static_cast<mqd_t>(descriptor_));
        descriptor_ = -1;
    }
    if (owner_ && !name_.empty()) {
        ::mq_unlink(name_.c_str());
    }
    name_.clear();
    maxMessageSize_ = 0;
    owner_ = false;
}

void PosixMessageQueue::unlink()
{
    if (!name_.empty()) {
        ::mq_unlink(name_.c_str());
    }
    owner_ = false;
}

} // namespace elap::ipc
