#include "elap/config/DatabaseConfiguration.hpp"
#include "elap/storage/Database.hpp"
#include "elap/storage/PersistentStore.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* testDbPath = "/tmp/elap_storage_test.db";

void cleanupTestDb()
{
    std::remove(testDbPath);
}

void testDatabaseOpenClose()
{
    cleanupTestDb();
    elap::storage::Database db;
    std::string error;
    assert(!db.isOpen());
    assert(db.open(testDbPath, &error));
    assert(db.isOpen());
    assert(db.path() == testDbPath);
    db.close();
    assert(!db.isOpen());
    std::cout << "PASS: testDatabaseOpenClose" << std::endl;
}

void testDatabaseMove()
{
    cleanupTestDb();
    elap::storage::Database db;
    assert(db.open(testDbPath));

    elap::storage::Database db2 = std::move(db);
    assert(db2.isOpen());
    assert(!db.isOpen());

    elap::storage::Database db3;
    db3 = std::move(db2);
    assert(db3.isOpen());
    assert(!db2.isOpen());

    db3.close();
    std::cout << "PASS: testDatabaseMove" << std::endl;
}

void testDatabaseExecute()
{
    cleanupTestDb();
    elap::storage::Database db;
    assert(db.open(testDbPath));
    assert(db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)"));
    assert(db.execute("INSERT INTO test (id, name) VALUES (1, 'hello')"));
    assert(db.execute("INSERT INTO test (id, name) VALUES (2, 'world')"));
    db.close();
    std::cout << "PASS: testDatabaseExecute" << std::endl;
}

void testDatabaseQuery()
{
    cleanupTestDb();
    elap::storage::Database db;
    assert(db.open(testDbPath));
    assert(db.execute("CREATE TABLE test (id INTEGER, name TEXT)"));
    assert(db.execute("INSERT INTO test VALUES (1, 'alpha')"));
    assert(db.execute("INSERT INTO test VALUES (2, 'beta')"));

    std::vector<std::string> names;
    db.query("SELECT name FROM test ORDER BY id",
             [&names](int argc, char** argv, char**) -> bool {
                 if (argc >= 1 && argv[0]) {
                     names.emplace_back(argv[0]);
                 }
                 return true;
             });
    assert(names.size() == 2);
    assert(names[0] == "alpha");
    assert(names[1] == "beta");

    db.close();
    std::cout << "PASS: testDatabaseQuery" << std::endl;
}

void testDatabaseExecWithArgs()
{
    cleanupTestDb();
    elap::storage::Database db;
    assert(db.open(testDbPath));
    assert(db.execute("CREATE TABLE test (key TEXT, value TEXT)"));

    const char* kvals[] = {"k1", "v1"};
    assert(db.execWithArgs("INSERT INTO test (key, value) VALUES (?1, ?2)", 2, kvals));

    std::string found;
    db.query("SELECT value FROM test WHERE key = 'k1'",
             [&found](int, char** argv, char**) -> bool {
                 if (argv[0]) found = argv[0];
                 return true;
             });
    assert(found == "v1");

    db.close();
    std::cout << "PASS: testDatabaseExecWithArgs" << std::endl;
}

void testDatabaseInvalidPath()
{
    elap::storage::Database db;
    std::string error;
    assert(!db.open("/nonexistent/path/db.sqlite", &error));
    assert(!error.empty());
    std::cout << "PASS: testDatabaseInvalidPath" << std::endl;
}

void testDatabaseReopen()
{
    cleanupTestDb();
    {
        elap::storage::Database db;
        assert(db.open(testDbPath));
        assert(db.execute("CREATE TABLE test (id INTEGER)"));
        assert(db.execute("INSERT INTO test VALUES (42)"));
    }
    {
        elap::storage::Database db;
        assert(db.open(testDbPath));
        int count = 0;
        db.query("SELECT COUNT(*) FROM test",
                 [&count](int, char** argv, char**) -> bool {
                     if (argv[0]) count = std::atoi(argv[0]);
                     return true;
                 });
        assert(count == 1);
    }
    std::cout << "PASS: testDatabaseReopen" << std::endl;
}

void testPersistentStorePutGet()
{
    cleanupTestDb();
    elap::storage::PersistentStore store;
    assert(store.open(testDbPath));
    assert(store.put("key1", "value1"));
    assert(store.put("key2", "value2"));

    auto v1 = store.get("key1");
    assert(v1.has_value());
    assert(*v1 == "value1");

    auto v2 = store.get("key2");
    assert(v2.has_value());
    assert(*v2 == "value2");

    auto missing = store.get("nonexistent");
    assert(!missing.has_value());

    store.close();
    std::cout << "PASS: testPersistentStorePutGet" << std::endl;
}

void testPersistentStoreOverwrite()
{
    cleanupTestDb();
    elap::storage::PersistentStore store;
    assert(store.open(testDbPath));
    assert(store.put("key", "old"));
    assert(store.put("key", "new"));
    auto val = store.get("key");
    assert(val.has_value());
    assert(*val == "new");
    store.close();
    std::cout << "PASS: testPersistentStoreOverwrite" << std::endl;
}

void testPersistentStoreRemove()
{
    cleanupTestDb();
    elap::storage::PersistentStore store;
    assert(store.open(testDbPath));
    assert(store.put("key", "value"));
    assert(store.has("key"));
    assert(store.remove("key"));
    assert(!store.has("key"));
    store.close();
    std::cout << "PASS: testPersistentStoreRemove" << std::endl;
}

void testPersistentStoreKeys()
{
    cleanupTestDb();
    elap::storage::PersistentStore store;
    assert(store.open(testDbPath));
    assert(store.put("c", "3"));
    assert(store.put("a", "1"));
    assert(store.put("b", "2"));

    auto keys = store.keys();
    assert(keys.size() == 3);
    assert(keys[0] == "a");
    assert(keys[1] == "b");
    assert(keys[2] == "c");

    store.close();
    std::cout << "PASS: testPersistentStoreKeys" << std::endl;
}

void testPersistentStoreClear()
{
    cleanupTestDb();
    elap::storage::PersistentStore store;
    assert(store.open(testDbPath));
    assert(store.put("a", "1"));
    assert(store.put("b", "2"));
    assert(store.clear());
    assert(store.keys().empty());
    store.close();
    std::cout << "PASS: testPersistentStoreClear" << std::endl;
}

void testPersistentStoreReopen()
{
    cleanupTestDb();
    {
        elap::storage::PersistentStore store;
        assert(store.open(testDbPath));
        assert(store.put("persist", "yes"));
    }
    {
        elap::storage::PersistentStore store;
        assert(store.open(testDbPath));
        auto val = store.get("persist");
        assert(val.has_value());
        assert(*val == "yes");
    }
    std::cout << "PASS: testPersistentStoreReopen" << std::endl;
}

void testPersistentStoreReportsClosedReadErrors()
{
    elap::storage::PersistentStore store;
    std::string error;

    assert(!store.get("missing", &error).has_value());
    assert(!error.empty());

    error.clear();
    assert(!store.has("missing", &error));
    assert(!error.empty());

    error.clear();
    assert(store.keys(&error).empty());
    assert(!error.empty());

    std::cout << "PASS: testPersistentStoreReportsClosedReadErrors" << std::endl;
}

void testDatabaseConfigSetGet()
{
    cleanupTestDb();
    elap::config::DatabaseConfiguration config;
    assert(config.open(testDbPath));

    assert(config.setString("name", "test_service"));
    assert(config.setInt("port", 8080));
    assert(config.setBool("debug", true));

    assert(config.has("name"));
    assert(config.getString("name", "") == "test_service");
    assert(config.getInt("port", 0) == 8080);
    assert(config.getBool("debug", false) == true);

    assert(config.getString("missing", "default") == "default");
    assert(config.getInt("missing", 42) == 42);
    assert(config.getBool("missing", false) == false);

    config.close();
    std::cout << "PASS: testDatabaseConfigSetGet" << std::endl;
}

void testDatabaseConfigReopen()
{
    cleanupTestDb();
    {
        elap::config::DatabaseConfiguration config;
        assert(config.open(testDbPath));
        assert(config.setString("device.name", "controller-1"));
        assert(config.setInt("startup.delay", 5));
        assert(config.setBool("logging.enabled", true));
    }
    {
        elap::config::DatabaseConfiguration config;
        assert(config.open(testDbPath));
        assert(config.getString("device.name", "default") == "controller-1");
        assert(config.getInt("startup.delay", 0) == 5);
        assert(config.getBool("logging.enabled", false) == true);
    }
    std::cout << "PASS: testDatabaseConfigReopen" << std::endl;
}

void testDatabaseConfigRemove()
{
    cleanupTestDb();
    elap::config::DatabaseConfiguration config;
    assert(config.open(testDbPath));
    assert(config.setString("key", "val"));
    assert(config.has("key"));
    assert(config.remove("key"));
    assert(!config.has("key"));
    config.close();
    std::cout << "PASS: testDatabaseConfigRemove" << std::endl;
}

void testDatabaseConfigLoadFromFile()
{
    cleanupTestDb();
    const char* seedPath = "/tmp/elap_storage_test_seed.conf";
    {
        std::ofstream f(seedPath);
        f << "# comment\n";
        f << "service.name = seeded_service\n";
        f << "log.level = debug\n";
        f << "worker.count = 4\n";
        f << "\n";
    }

    elap::config::DatabaseConfiguration config;
    assert(config.open(testDbPath));

    std::string error;
    bool loaded = config.loadFromFile(seedPath, &error);
    assert(loaded);

    assert(config.getString("service.name", "") == "seeded_service");
    assert(config.getString("log.level", "") == "debug");
    assert(config.getInt("worker.count", 0) == 4);

    config.close();
    std::remove(seedPath);
    std::cout << "PASS: testDatabaseConfigLoadFromFile" << std::endl;
}

void testDatabaseConfigLoadFromFileDoesNotPartiallySeed()
{
    cleanupTestDb();
    const char* seedPath = "/tmp/elap_storage_test_bad_seed.conf";
    {
        std::ofstream f(seedPath);
        f << "existing = changed\n";
        f << "new.value = should_not_persist\n";
        f << "invalid line\n";
    }

    elap::config::DatabaseConfiguration config;
    assert(config.open(testDbPath));
    assert(config.setString("existing", "original"));

    std::string error;
    assert(!config.loadFromFile(seedPath, &error));
    assert(!error.empty());

    assert(config.getString("existing", "") == "original");
    assert(config.getString("new.value", "missing") == "missing");

    config.close();
    std::remove(seedPath);
    std::cout << "PASS: testDatabaseConfigLoadFromFileDoesNotPartiallySeed" << std::endl;
}

void testDatabaseConfigIntConversions()
{
    cleanupTestDb();
    elap::config::DatabaseConfiguration config;
    assert(config.open(testDbPath));

    assert(config.setString("bad_int", "not_a_number"));
    assert(config.getInt("bad_int", 99) == 99);

    assert(config.setString("good_int", "123"));
    assert(config.getInt("good_int", 0) == 123);

    assert(config.setString("bool_true", "yes"));
    assert(config.getBool("bool_true", false) == true);

    assert(config.setString("bool_false", "off"));
    assert(config.getBool("bool_false", true) == false);

    assert(config.setString("bool_bad", "maybe"));
    assert(config.getBool("bool_bad", true) == true);

    config.close();
    std::cout << "PASS: testDatabaseConfigIntConversions" << std::endl;
}

} // namespace

void runStorageTests()
{
    testDatabaseOpenClose();
    testDatabaseMove();
    testDatabaseExecute();
    testDatabaseQuery();
    testDatabaseExecWithArgs();
    testDatabaseInvalidPath();
    testDatabaseReopen();

    testPersistentStorePutGet();
    testPersistentStoreOverwrite();
    testPersistentStoreRemove();
    testPersistentStoreKeys();
    testPersistentStoreClear();
    testPersistentStoreReopen();
    testPersistentStoreReportsClosedReadErrors();

    testDatabaseConfigSetGet();
    testDatabaseConfigReopen();
    testDatabaseConfigRemove();
    testDatabaseConfigLoadFromFile();
    testDatabaseConfigLoadFromFileDoesNotPartiallySeed();
    testDatabaseConfigIntConversions();

    cleanupTestDb();
    std::cout << "All storage tests passed." << std::endl;
}

struct StorageTestRunner {
    StorageTestRunner()
    {
        runStorageTests();
    }
} storageTestRunner;
