#include "elap/storage/Database.hpp"

#include <chrono>
#include <cstring>
#include <limits>
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

class Statement {
public:
    Statement() = default;
    ~Statement()
    {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt** out()
    {
        return &stmt_;
    }

    sqlite3_stmt* get() const
    {
        return stmt_;
    }

private:
    sqlite3_stmt* stmt_ {nullptr};
};

bool validArgumentArrays(int argc,
                         const char* const* values,
                         const int* sizes,
                         const Database::BindType* types,
                         std::string* errorMessage)
{
    if (argc < 0) {
        setError(errorMessage, "argument count must not be negative");
        return false;
    }
    if (argc > 0 && values == nullptr) {
        setError(errorMessage, "argument values array is null");
        return false;
    }
    if (sizes == nullptr || types == nullptr) {
        return true;
    }
    for (int i = 0; i < argc; ++i) {
        if (values[i] != nullptr && sizes[i] < 0) {
            setError(errorMessage, "argument size must not be negative");
            return false;
        }
    }
    return true;
}

bool bindTextArgs(sqlite3* db,
                  sqlite3_stmt* stmt,
                  int argc,
                  const char* const* values,
                  std::string* errorMessage)
{
    if (!validArgumentArrays(argc, values, nullptr, nullptr, errorMessage)) {
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        const int rc = values[i] == nullptr
            ? sqlite3_bind_null(stmt, i + 1)
            : sqlite3_bind_text(stmt, i + 1, values[i], -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            setError(errorMessage, "bind failed: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
    }
    return true;
}

bool bindTypedArgs(sqlite3* db,
                   sqlite3_stmt* stmt,
                   int argc,
                   const char* const* values,
                   const int* sizes,
                   const Database::BindType* types,
                   std::string* errorMessage)
{
    if (!validArgumentArrays(argc, values, sizes, types, errorMessage)) {
        return false;
    }
    if (argc > 0 && (sizes == nullptr || types == nullptr)) {
        setError(errorMessage, "typed bind metadata is null");
        return false;
    }

    for (int i = 0; i < argc; ++i) {
        int rc = SQLITE_OK;
        if (values[i] == nullptr) {
            rc = sqlite3_bind_null(stmt, i + 1);
        } else if (types[i] == Database::BindType::Blob) {
            rc = sqlite3_bind_blob(stmt, i + 1, values[i], sizes[i], SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_text(stmt, i + 1, values[i], sizes[i], SQLITE_TRANSIENT);
        }
        if (rc != SQLITE_OK) {
            setError(errorMessage, "bind failed: " + std::string(sqlite3_errmsg(db)));
            return false;
        }
    }
    return true;
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
    sqlite3_busy_timeout(db_, static_cast<int>(std::chrono::milliseconds(1000).count()));
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

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        const int argc = sqlite3_column_count(stmt.get());
        std::vector<const char*> colValues(argc);
        std::vector<const char*> colNames(argc);
        for (int i = 0; i < argc; ++i) {
            colValues[i] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), i));
            colNames[i] = sqlite3_column_name(stmt.get(), i);
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

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    if (!bindTextArgs(db_, stmt.get(), argc, values, errorMessage)) {
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt.get());
        std::vector<const char*> colValues(colCount);
        std::vector<const char*> colNames(colCount);
        for (int i = 0; i < colCount; ++i) {
            colValues[i] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), i));
            colNames[i] = sqlite3_column_name(stmt.get(), i);
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

    return ok;
}

bool Database::queryBlob(const std::string& sql, BlobRowCallback callback,
                         std::string* errorMessage) const
{
    if (db_ == nullptr) {
        setError(errorMessage, "database is not open");
        return false;
    }

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt.get());
        std::vector<Column> columns(colCount);
        for (int i = 0; i < colCount; ++i) {
            columns[i].data = sqlite3_column_blob(stmt.get(), i);
            columns[i].size = sqlite3_column_bytes(stmt.get(), i);
            columns[i].name = sqlite3_column_name(stmt.get(), i);
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

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare query failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    if (!bindTextArgs(db_, stmt.get(), argc, values, errorMessage)) {
        return false;
    }

    bool ok = true;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        const int colCount = sqlite3_column_count(stmt.get());
        std::vector<Column> columns(colCount);
        for (int i = 0; i < colCount; ++i) {
            columns[i].data = sqlite3_column_blob(stmt.get(), i);
            columns[i].size = sqlite3_column_bytes(stmt.get(), i);
            columns[i].name = sqlite3_column_name(stmt.get(), i);
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

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    if (!bindTextArgs(db_, stmt.get(), argc, values, errorMessage)) {
        return false;
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "exec failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

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

    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt.out(), nullptr);
    if (rc != SQLITE_OK) {
        setError(errorMessage, "prepare failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    if (!bindTypedArgs(db_, stmt.get(), argc, values, sizes, types, errorMessage)) {
        return false;
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setError(errorMessage, "exec bind failed: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

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
