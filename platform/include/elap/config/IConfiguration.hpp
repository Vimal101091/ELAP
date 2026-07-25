#pragma once

#include <string>

namespace elap::config {

class IConfiguration {
public:
    virtual ~IConfiguration() = default;

    virtual bool has(const std::string& key) const = 0;
    virtual std::string getString(const std::string& key,
                                  const std::string& defaultValue) const = 0;
    virtual int getInt(const std::string& key, int defaultValue) const = 0;
    virtual bool getBool(const std::string& key, bool defaultValue) const = 0;
};

} // namespace elap::config
