# ELAP Phase 3 Storage Design

## Scope

Phase 3 introduces persistent data storage to ELAP using SQLite. The
implementation provides three layers built on top of each other:

1. SQLite RAII wrapper
2. Persistent key-value/blob store
3. Database-backed configuration

## Components

### Database (SQLite Wrapper)

`elap::storage::Database` provides a move-only RAII wrapper around a
`sqlite3*` connection handle. It follows the same ownership and error
handling patterns established by `SharedMemoryRegion` in Phase 2.

Responsibilities:

- Open and close SQLite databases
- Execute SQL statements without results
- Execute parameterized queries with row callbacks
- Execute parameterized statements with text bindings
- Execute parameterized statements with blob bindings
- Report errors through `std::string*` out-parameter
- Support move construction and move assignment

The wrapper uses `sqlite3_open_v2` with `SQLITE_OPEN_READWRITE |
SQLITE_OPEN_CREATE` so that a database file is created automatically if
it does not exist.

### PersistentStore

`elap::storage::PersistentStore` builds on `Database` to provide a
schema-managed key-value store backed by a single SQLite table.

Schema:

```sql
CREATE TABLE IF NOT EXISTS kv_store (
    key   TEXT PRIMARY KEY,
    value BLOB NOT NULL
)
```

Responsibilities:

- Put and get string values
- Put multiple string values atomically
- Put and get binary blob values
- Check key existence
- Remove individual keys
- List all keys in sorted order
- Clear all entries
- Ensure schema is created on first open

The store uses prepared statements with parameter binding for safe
inserts and queries. Batch string writes use a SQLite transaction so a
failed write rolls back the whole batch.

### DatabaseConfiguration

`elap::config::DatabaseConfiguration` implements the existing
`IConfiguration` interface backed by `PersistentStore`. This allows
services to use the same configuration API whether the underlying storage
is a flat file or a SQLite database.

Responsibilities:

- Implement `has()`, `getString()`, `getInt()`, `getBool()` from
  `IConfiguration`
- Provide mutable `setString()`, `setInt()`, `setBool()`, `remove()`
  operations
- Seed configuration from a flat key-value file using `loadFromFile()`
- Validate seed files before writing and commit seed data atomically
- Parse integer and boolean values with the same conventions as
  `KeyValueConfiguration`

## Sample Executables

### storage\_demo

Demonstrates the `PersistentStore` API directly:

- Opens a database file
- Stores and retrieves string values
- Stores and retrieves a binary blob
- Lists all keys
- Removes a key

### storage\_config\_service

Demonstrates `DatabaseConfiguration` inside an ELAP service:

- Full `IService` lifecycle
- Loads configuration from a SQLite database
- Optionally seeds from a flat file via `ELAP_STORAGE_CONFIG_SEED`
- Starts heartbeat worker threads using config values

## Integration

The storage module is added to the `elap_platform` static library. SQLite
is included as an amalgamation source under `third_party/sqlite3/` so
that no external development package is required.

## Current Tests

Unit coverage validates:

- Database open, close, move semantics
- SQL execute, query with callback, parameterized execute
- Database reopen preserves data
- Invalid path error handling
- PersistentStore put/get string
- PersistentStore overwrite existing key
- PersistentStore remove
- PersistentStore keys listing (sorted)
- PersistentStore clear
- PersistentStore reopen preserves data
- PersistentStore closed-read error reporting
- DatabaseConfiguration set/get string, int, bool
- DatabaseConfiguration remove
- DatabaseConfiguration load from file
- DatabaseConfiguration rejects invalid seed files without partial writes
- DatabaseConfiguration type conversion edge cases

Integration coverage validates:

- storage\_demo produces expected output for all operations

## SQLite Amalgamation

The SQLite source is bundled as `third_party/sqlite3/sqlite3.c` and
`third_party/sqlite3/sqlite3.h`. This avoids requiring
`libsqlite3-dev` on the development host and ensures a consistent SQLite
version across build environments.

## Next Phase 3 Activities

Recommended next activities:

1. Add WAL mode support for concurrent read access
2. Add a reusable `Database::transaction()` helper for broader
   multi-statement transaction use
3. Add schema versioning and migration support to `PersistentStore`
4. Add a `DatabaseConfiguration::keys()` method for full config
   enumeration
5. Add thread safety documentation and optional mutex wrapper
