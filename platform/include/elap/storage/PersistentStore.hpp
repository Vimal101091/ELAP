#pragma once

#include "elap/storage/Database.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <vector>

namespace elap::storage {

class PersistentStore {
public:
    bool open(const std::string& dbPath, std::string* errorMessage = nullptr);
    void close();
    bool isOpen() const;

    bool put(const std::string& key, const std::string& value,
             std::string* errorMessage = nullptr);
    bool putAll(const std::vector<std::pair<std::string, std::string>>& values,
                std::string* errorMessage = nullptr);
    std::optional<std::string> get(const std::string& key,
                                   std::string* errorMessage = nullptr) const;
    bool remove(const std::string& key, std::string* errorMessage = nullptr);
    bool has(const std::string& key, std::string* errorMessage = nullptr) const;

    bool putBlob(const std::string& key, const void* data, std::size_t size,
                 std::string* errorMessage = nullptr);
    std::vector<uint8_t> getBlob(const std::string& key,
                                 std::string* errorMessage = nullptr) const;

    std::vector<std::string> keys(std::string* errorMessage = nullptr) const;
    bool clear(std::string* errorMessage = nullptr);

private:
    Database db_;
    bool ensureSchema(std::string* errorMessage);
};

} // namespace elap::storage
