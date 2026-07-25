#pragma once

#include "elap/config/IConfiguration.hpp"

#include <string>
#include <unordered_map>

namespace elap::config {

class KeyValueConfiguration final : public IConfiguration {
public:
    bool loadFromFile(const std::string& path, std::string* errorMessage = nullptr);

    bool has(const std::string& key) const override;
    std::string getString(const std::string& key,
                          const std::string& defaultValue) const override;
    int getInt(const std::string& key, int defaultValue) const override;
    bool getBool(const std::string& key, bool defaultValue) const override;

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace elap::config
