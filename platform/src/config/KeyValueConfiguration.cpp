#include "elap/config/KeyValueConfiguration.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace elap::config {
namespace {

std::string trim(std::string text)
{
    const auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };

    text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
    text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
    return text;
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

} // namespace

bool KeyValueConfiguration::loadFromFile(const std::string& path, std::string* errorMessage)
{
    std::ifstream file(path);
    if (!file) {
        if (errorMessage != nullptr) {
            *errorMessage = "failed to open configuration file: " + path;
        }
        return false;
    }

    values_.clear();

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const auto commentPosition = line.find('#');
        if (commentPosition != std::string::npos) {
            line.erase(commentPosition);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            if (errorMessage != nullptr) {
                std::ostringstream stream;
                stream << "invalid configuration line " << lineNumber << ": expected key=value";
                *errorMessage = stream.str();
            }
            values_.clear();
            return false;
        }

        auto key = trim(line.substr(0, separator));
        auto value = trim(line.substr(separator + 1));
        if (key.empty()) {
            if (errorMessage != nullptr) {
                std::ostringstream stream;
                stream << "invalid configuration line " << lineNumber << ": key is empty";
                *errorMessage = stream.str();
            }
            values_.clear();
            return false;
        }

        values_[std::move(key)] = std::move(value);
    }

    return true;
}

bool KeyValueConfiguration::has(const std::string& key) const
{
    return values_.find(key) != values_.end();
}

std::string KeyValueConfiguration::getString(const std::string& key,
                                             const std::string& defaultValue) const
{
    const auto iterator = values_.find(key);
    return iterator == values_.end() ? defaultValue : iterator->second;
}

int KeyValueConfiguration::getInt(const std::string& key, int defaultValue) const
{
    const auto iterator = values_.find(key);
    if (iterator == values_.end()) {
        return defaultValue;
    }

    try {
        std::size_t parsedCharacters = 0;
        const int value = std::stoi(iterator->second, &parsedCharacters);
        return parsedCharacters == iterator->second.size() ? value : defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

bool KeyValueConfiguration::getBool(const std::string& key, bool defaultValue) const
{
    const auto iterator = values_.find(key);
    if (iterator == values_.end()) {
        return defaultValue;
    }

    const auto value = lower(trim(iterator->second));
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return defaultValue;
}

} // namespace elap::config
