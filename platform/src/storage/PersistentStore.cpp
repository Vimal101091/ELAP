#include "elap/storage/PersistentStore.hpp"

#include <limits>

namespace elap::storage {
namespace {

void restoreError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

bool PersistentStore::open(const std::string& dbPath, std::string* errorMessage)
{
    if (!db_.open(dbPath, errorMessage)) {
        return false;
    }
    if (!ensureSchema(errorMessage)) {
        db_.close();
        return false;
    }
    return true;
}

void PersistentStore::close()
{
    db_.close();
}

bool PersistentStore::isOpen() const
{
    return db_.isOpen();
}

bool PersistentStore::ensureSchema(std::string* errorMessage)
{
    return db_.execute(
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "  key   TEXT PRIMARY KEY,"
        "  value BLOB NOT NULL"
        ")",
        errorMessage);
}

bool PersistentStore::put(const std::string& key, const std::string& value,
                          std::string* errorMessage)
{
    const char* values[] = {key.c_str(), value.c_str()};
    return db_.execWithArgs(
        "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?1, ?2)",
        2, values, errorMessage);
}

bool PersistentStore::putAll(const std::vector<std::pair<std::string, std::string>>& values,
                             std::string* errorMessage)
{
    if (!db_.execute("BEGIN IMMEDIATE TRANSACTION", errorMessage)) {
        return false;
    }

    for (const auto& [key, value] : values) {
        if (!put(key, value, errorMessage)) {
            const std::string originalError = errorMessage != nullptr ? *errorMessage : "";
            std::string rollbackError;
            db_.execute("ROLLBACK", &rollbackError);
            restoreError(errorMessage, originalError);
            return false;
        }
    }

    if (!db_.execute("COMMIT", errorMessage)) {
        const std::string originalError = errorMessage != nullptr ? *errorMessage : "";
        std::string rollbackError;
        db_.execute("ROLLBACK", &rollbackError);
        restoreError(errorMessage, originalError);
        return false;
    }

    return true;
}

std::optional<std::string> PersistentStore::get(const std::string& key,
                                                std::string* errorMessage) const
{
    std::optional<std::string> result;
    bool found = false;
    const char* values[] = {key.c_str()};

    const bool ok = db_.queryWithArgs(
        "SELECT value FROM kv_store WHERE key = ?1",
        1, values,
        [&result, &found](int argc, char** argv, char**) -> bool {
            if (argc >= 1 && argv[0] != nullptr) {
                result = std::string(argv[0]);
                found = true;
            }
            return true;
        },
        errorMessage);

    return ok && found ? result : std::optional<std::string>{};
}

bool PersistentStore::remove(const std::string& key, std::string* errorMessage)
{
    const char* values[] = {key.c_str()};
    return db_.execWithArgs(
        "DELETE FROM kv_store WHERE key = ?1",
        1, values, errorMessage);
}

bool PersistentStore::has(const std::string& key, std::string* errorMessage) const
{
    bool found = false;
    const char* values[] = {key.c_str()};

    const bool ok = db_.queryWithArgs(
        "SELECT 1 FROM kv_store WHERE key = ?1",
        1, values,
        [&found](int, char**, char**) -> bool {
            found = true;
            return true;
        },
        errorMessage);
    return ok && found;
}

bool PersistentStore::putBlob(const std::string& key, const void* data, std::size_t size,
                              std::string* errorMessage)
{
    if (data == nullptr && size > 0) {
        setError(errorMessage, "putBlob failed: data is null");
        return false;
    }
    if (key.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        setError(errorMessage, "putBlob failed: key or blob is too large");
        return false;
    }

    const char emptyBlob[] = "";
    const char* values[] = {
        key.c_str(),
        size == 0 && data == nullptr ? emptyBlob : static_cast<const char*>(data)
    };
    const int sizes[] = {static_cast<int>(key.size()), static_cast<int>(size)};
    const Database::BindType types[] = {Database::BindType::Text, Database::BindType::Blob};
    return db_.execBind(
        "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?1, ?2)",
        2, values, sizes, types, errorMessage);
}

std::vector<uint8_t> PersistentStore::getBlob(const std::string& key,
                                              std::string* errorMessage) const
{
    std::vector<uint8_t> result;
    const char* values[] = {key.c_str()};

    const bool ok = db_.queryBlobWithArgs(
        "SELECT value FROM kv_store WHERE key = ?1",
        1, values,
        [&result](int argc, Database::Column* columns) -> bool {
            if (argc >= 1 && columns[0].data != nullptr && columns[0].size > 0) {
                const auto* data = static_cast<const uint8_t*>(columns[0].data);
                result.assign(data, data + columns[0].size);
            }
            return true;
        },
        errorMessage);
    if (!ok) {
        result.clear();
    }
    return result;
}

std::vector<std::string> PersistentStore::keys(std::string* errorMessage) const
{
    std::vector<std::string> result;
    const bool ok = db_.query(
        "SELECT key FROM kv_store ORDER BY key",
        [&result](int argc, char** argv, char**) -> bool {
            if (argc >= 1 && argv[0] != nullptr) {
                result.emplace_back(argv[0]);
            }
            return true;
        },
        errorMessage);
    if (!ok) {
        result.clear();
    }
    return result;
}

bool PersistentStore::clear(std::string* errorMessage)
{
    return db_.execute("DELETE FROM kv_store", errorMessage);
}

} // namespace elap::storage
