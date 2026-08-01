#include "elap/config/DatabaseConfiguration.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

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

bool DatabaseConfiguration::open(const std::string& dbPath, std::string* errorMessage)
{
    return store_.open(dbPath, errorMessage);
}

void DatabaseConfiguration::close()
{
    store_.close();
}

bool DatabaseConfiguration::isOpen() const
{
    return store_.isOpen();
}

bool DatabaseConfiguration::loadFromFile(const std::string& path, std::string* errorMessage)
{
    std::ifstream file(path);
    if (!file) {
        if (errorMessage != nullptr) {
            *errorMessage = "failed to open configuration file: " + path;
        }
        return false;
    }

    std::string line;
    int lineNumber = 0;
    std::vector<std::pair<std::string, std::string>> values;
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
            return false;
        }

        values.emplace_back(std::move(key), std::move(value));
    }

    return store_.putAll(values, errorMessage);
}

bool DatabaseConfiguration::setString(const std::string& key, const std::string& value,
                                     std::string* errorMessage)
{
    return store_.put(key, value, errorMessage);
}

bool DatabaseConfiguration::setInt(const std::string& key, int value,
                                  std::string* errorMessage)
{
    return store_.put(key, std::to_string(value), errorMessage);
}

bool DatabaseConfiguration::setBool(const std::string& key, bool value,
                                   std::string* errorMessage)
{
    return store_.put(key, value ? "true" : "false", errorMessage);
}

bool DatabaseConfiguration::remove(const std::string& key, std::string* errorMessage)
{
    return store_.remove(key, errorMessage);
}

bool DatabaseConfiguration::has(const std::string& key) const
{
    return store_.has(key);
}

std::string DatabaseConfiguration::getString(const std::string& key,
                                            const std::string& defaultValue) const
{
    auto result = store_.get(key);
    return result.has_value() ? *result : defaultValue;
}

int DatabaseConfiguration::getInt(const std::string& key, int defaultValue) const
{
    auto result = store_.get(key);
    if (!result.has_value()) {
        return defaultValue;
    }

    try {
        std::size_t parsedCharacters = 0;
        const int value = std::stoi(*result, &parsedCharacters);
        return parsedCharacters == result->size() ? value : defaultValue;
    } catch (...) {
        return defaultValue;
    }
}

bool DatabaseConfiguration::getBool(const std::string& key, bool defaultValue) const
{
    auto result = store_.get(key);
    if (!result.has_value()) {
        return defaultValue;
    }

    const auto value = lower(trim(*result));
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return defaultValue;
}

} // namespace elap::config
