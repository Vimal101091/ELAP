#include "elap/ipc/EventBus.hpp"

namespace elap::ipc {
namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool encodeEvent(const Event& event, std::string& encoded, std::string* errorMessage)
{
    if (event.topic.empty()) {
        setError(errorMessage, "event topic is empty");
        return false;
    }
    if (event.topic.find('\n') != std::string::npos) {
        setError(errorMessage, "event topic must not contain newline");
        return false;
    }

    encoded = event.topic + '\n' + event.payload;
    return true;
}

bool decodeEvent(const std::string& encoded, Event& event, std::string* errorMessage)
{
    const auto separator = encoded.find('\n');
    if (separator == std::string::npos) {
        setError(errorMessage, "event message is missing topic separator");
        return false;
    }

    event.topic = encoded.substr(0, separator);
    event.payload = encoded.substr(separator + 1);
    if (event.topic.empty()) {
        setError(errorMessage, "event topic is empty");
        return false;
    }
    return true;
}

} // namespace

bool MessageQueueEventBus::create(const std::string& name,
                                  long maxEvents,
                                  long maxEventSize,
                                  std::string* errorMessage)
{
    return queue_.create(name, maxEvents, maxEventSize, errorMessage);
}

bool MessageQueueEventBus::open(const std::string& name, std::string* errorMessage)
{
    return queue_.open(name, errorMessage);
}

bool MessageQueueEventBus::publish(const Event& event, std::string* errorMessage)
{
    std::string encoded;
    if (!encodeEvent(event, encoded, errorMessage)) {
        return false;
    }
    return queue_.sendMessage(encoded, 0, errorMessage);
}

bool MessageQueueEventBus::receive(Event& event, std::string* errorMessage)
{
    std::string encoded;
    if (!queue_.receiveMessage(encoded, nullptr, errorMessage)) {
        return false;
    }
    return decodeEvent(encoded, event, errorMessage);
}

bool MessageQueueEventBus::isOpen() const
{
    return queue_.isOpen();
}

void MessageQueueEventBus::close()
{
    queue_.close();
}

void MessageQueueEventBus::unlink()
{
    queue_.unlink();
}

} // namespace elap::ipc
