#include "elap/storage/Database.hpp"

#include <cstring>
#include <sqlite3.h>
#include <utility>

namespace elap::storage {
namespace {

void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

} // namespace

Database::~Database()
{
    close();
}

Database::Database(Database&& other) noexcept
    : db_(other.db_)
    , path_(std::move(other.path_))
{
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other) {
        close();
        db_ = other.db_;
        path_ = std::move(other.path_);
        other.db_ = nullptr;
    }
    return *this;
}

bool Database::open(const std::string& path, std::string* errorMessage)
{
    close();
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(path.c_str(), &db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                   nullptr);
    if (rc != SQLITE_OK) {
        if (db != nullptr) {
            sqlite3_close(db);
        }
        setError(errorMessage, "failed to open database '" + path + "': " + sqlite3_errstr(rc));
        return false;
    }
    db_ = db;
    path_ = path;
    return true;
}

void Database::close()
{
    if (db_ != nullptr) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
    path_.clear();
}

bool Database::isOpen() const
{
    return db_ != nullptr;
}

const std::string& Database::path() const
{
    return path_;
}

bool Database::execute(const std::string& sql, std::string* errorMessage)
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = "execute failed: ";
        if (errMsg != nullptr) {
            msg += errMsg;
            sqlite3_free(errMsg);
        } else {
            msg += sqlite3_errstr(rc);
        }
        setError(errorMessage, msg);
        return false;
    }
    return true;
}

bool Database::query(const std::string& sql, RowCallback callback,
                     std::string* errorMessage) const
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const int argc = sqlite3_column_count(stmt);
        std::vector<const char*> colValues(argc);
        std::vector<const char*> colNames(argc);
        for (int i = 0; i < argc; ++i) {
            colValues[i] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            colNames[i] = sqlite3_column_name(stmt, i);
        }
        if (!callback(argc,
                      const_cast<char**>(colValues.data()),
                      const_cast<char**>(colNames.data()))) {
            ok = false;
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "step query failed: " + std::string(sqlite3_errmsg(db_)));
        ok = false;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool Database::queryWithArgs(const std::string& sql,
                             int argc, const char* const* values,
                             RowCallback callback,
                             std::string* errorMessage) const
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        if (values[i] == nullptr) {
            sqlite3_bind_null(stmt, i + 1);
        } else {
            sqlite3_bind_text(stmt, i + 1, values[i], -1, SQLITE_TRANSIENT);
        }
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt);
        std::vector<const char*> colValues(colCount);
        std::vector<const char*> colNames(colCount);
        for (int i = 0; i < colCount; ++i) {
            colValues[i] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            colNames[i] = sqlite3_column_name(stmt, i);
        }
        if (!callback(colCount,
                      const_cast<char**>(colValues.data()),
                      const_cast<char**>(colNames.data()))) {
            ok = false;
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "step query failed: " + std::string(sqlite3_errmsg(db_)));
        ok = false;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool Database::queryBlob(const std::string& sql, BlobRowCallback callback,
                         std::string* errorMessage) const
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt);
        std::vector<Column> columns(colCount);
        for (int i = 0; i < colCount; ++i) {
            columns[i].data = sqlite3_column_blob(stmt, i);
            columns[i].size = sqlite3_column_bytes(stmt, i);
            columns[i].name = sqlite3_column_name(stmt, i);
        }
        if (!callback(colCount, columns.data())) {
            ok = false;
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "step query failed: " + std::string(sqlite3_errmsg(db_)));
        ok = false;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool Database::queryBlobWithArgs(const std::string& sql,
                                 int argc, const char* const* values,
                                 BlobRowCallback callback,
                                 std::string* errorMessage) const
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        if (values[i] == nullptr) {
            sqlite3_bind_null(stmt, i + 1);
        } else {
            sqlite3_bind_text(stmt, i + 1, values[i], -1, SQLITE_TRANSIENT);
        }
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt);
        std::vector<Column> columns(colCount);
        for (int i = 0; i < colCount; ++i) {
            columns[i].data = sqlite3_column_blob(stmt, i);
            columns[i].size = sqlite3_column_bytes(stmt, i);
            columns[i].name = sqlite3_column_name(stmt, i);
        }
        if (!callback(colCount, columns.data())) {
            ok = false;
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "step query failed: " + std::string(sqlite3_errmsg(db_)));
        ok = false;
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool Database::execWithArgs(const std::string& sql,
                            int argc, const char* const* values,
                            std::string* errorMessage)
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        if (values[i] == nullptr) {
            sqlite3_bind_null(stmt, i + 1);
        } else {
            sqlite3_bind_text(stmt, i + 1, values[i], -1, SQLITE_TRANSIENT);
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "exec failed: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::execBind(const std::string& sql,
                        int argc, const char* const* values,
                        const int* sizes, const BindType* types,
                        std::string* errorMessage)
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt, nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        if (values[i] == nullptr) {
            sqlite3_bind_null(stmt, i + 1);
        } else if (types[i] == BindType::Blob) {
            sqlite3_bind_blob(stmt, i + 1, values[i], sizes[i], SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_text(stmt, i + 1, values[i], sizes[i], SQLITE_TRANSIENT);
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "exec bind failed: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

int64_t Database::lastInsertRowId() const
{
    if (db_ == nullptr) {
        return 0;
    }
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const
{
    if (db_ == nullptr) {
        return 0;
    }
    return sqlite3_changes(db_);
}

} // namespace elap::storage
