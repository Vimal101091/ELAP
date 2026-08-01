#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct sqlite3;

namespace elap::storage {

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    bool open(const std::string& path, std::string* errorMessage = nullptr);
    void close();
    bool isOpen() const;
    const std::string& path() const;

    bool execute(const std::string& sql, std::string* errorMessage = nullptr);

    using RowCallback = std::function<bool(int argc, char** argv, char** colName)>;
    bool query(const std::string& sql, RowCallback callback,
               std::string* errorMessage = nullptr) const;

    struct Column {
        const void* data {nullptr};
        int size {0};
        const char* name {nullptr};
    };
    using BlobRowCallback = std::function<bool(int argc, Column* columns)>;
    bool queryBlob(const std::string& sql, BlobRowCallback callback,
                   std::string* errorMessage = nullptr) const;

    bool queryWithArgs(const std::string& sql,
                       int argc, const char* const* values,
                       RowCallback callback,
                       std::string* errorMessage = nullptr) const;

    bool queryBlobWithArgs(const std::string& sql,
                           int argc, const char* const* values,
                           BlobRowCallback callback,
                           std::string* errorMessage = nullptr) const;

    bool execWithArgs(const std::string& sql,
                      int argc, const char* const* values,
                      std::string* errorMessage = nullptr);

    enum class BindType : int { Text = 0, Blob = 1 };
    bool execBind(const std::string& sql,
                  int argc, const char* const* values,
                  const int* sizes, const BindType* types,
                  std::string* errorMessage = nullptr);

    int64_t lastInsertRowId() const;
    int changes() const;

private:
    sqlite3* db_ {nullptr};
    std::string path_;
};

} // namespace elap::storage
