#pragma once

#include "elap/config/IConfiguration.hpp"
#include "elap/storage/PersistentStore.hpp"

#include <string>

namespace elap::config {

class DatabaseConfiguration final : public IConfiguration {
public:
    bool open(const std::string& dbPath, std::string* errorMessage = nullptr);
    void close();
    bool isOpen() const;

    bool loadFromFile(const std::string& path, std::string* errorMessage = nullptr);

    bool setString(const std::string& key, const std::string& value,
                   std::string* errorMessage = nullptr);
    bool setInt(const std::string& key, int value,
                std::string* errorMessage = nullptr);
    bool setBool(const std::string& key, bool value,
                 std::string* errorMessage = nullptr);
    bool remove(const std::string& key, std::string* errorMessage = nullptr);

    bool has(const std::string& key) const override;
    std::string getString(const std::string& key,
                          const std::string& defaultValue) const override;
    int getInt(const std::string& key, int defaultValue) const override;
    bool getBool(const std::string& key, bool defaultValue) const override;

private:
    storage::PersistentStore store_;
};

} // namespace elap::config
