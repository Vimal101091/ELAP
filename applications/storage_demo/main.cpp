#include "elap/config/DatabaseConfiguration.hpp"
#include "elap/storage/PersistentStore.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
    const char* dbPath = std::getenv("ELAP_STORAGE_DEMO_DB");
    const std::string path = dbPath ? dbPath : "/tmp/elap_storage_demo.db";

    std::cout << "Opening database: " << path << std::endl;

    elap::storage::PersistentStore store;
    std::string error;
    if (!store.open(path, &error)) {
        std::cerr << "Failed to open store: " << error << std::endl;
        return 1;
    }

    if (!store.put("greeting", "hello world", &error)) {
        std::cerr << "put failed: " << error << std::endl;
        return 1;
    }

    if (!store.put("count", "42", &error)) {
        std::cerr << "put failed: " << error << std::endl;
        return 1;
    }

    if (!store.put("enabled", "true", &error)) {
        std::cerr << "put failed: " << error << std::endl;
        return 1;
    }

    auto greeting = store.get("greeting", &error);
    if (greeting.has_value()) {
        std::cout << "greeting = " << *greeting << std::endl;
    } else {
        std::cerr << "get greeting failed: " << error << std::endl;
        return 1;
    }

    auto count = store.get("count", &error);
    if (count.has_value()) {
        std::cout << "count = " << *count << std::endl;
    } else {
        std::cerr << "get count failed: " << error << std::endl;
        return 1;
    }

    auto enabled = store.get("enabled", &error);
    if (enabled.has_value()) {
        std::cout << "enabled = " << *enabled << std::endl;
    } else {
        std::cerr << "get enabled failed: " << error << std::endl;
        return 1;
    }

    if (!store.putBlob("binary", "\x00\x01\x02\x03", 4, &error)) {
        std::cerr << "putBlob failed: " << error << std::endl;
        return 1;
    }

    auto blob = store.getBlob("binary", &error);
    std::cout << "binary blob size = " << blob.size() << std::endl;

    auto allKeys = store.keys(&error);
    std::cout << "Keys (" << allKeys.size() << "):";
    for (const auto& k : allKeys) {
        std::cout << " " << k;
    }
    std::cout << std::endl;

    if (!store.remove("count", &error)) {
        std::cerr << "remove failed: " << error << std::endl;
        return 1;
    }

    std::cout << "has count after remove = " << (store.has("count") ? "true" : "false") << std::endl;

    store.close();
    elap::config::DatabaseConfiguration config;
    if (!config.open(path, &error)) {
        std::cerr << "Failed to open config: " << error << std::endl;
        return 1;
    }

    if (!config.setString("device.name", "controller-1", &error)
        || !config.setInt("startup.delay", 5, &error)
        || !config.setBool("logging.enabled", true, &error)) {
        std::cerr << "config write failed: " << error << std::endl;
        return 1;
    }
    config.close();

    elap::config::DatabaseConfiguration reopenedConfig;
    if (!reopenedConfig.open(path, &error)) {
        std::cerr << "Failed to reopen config: " << error << std::endl;
        return 1;
    }

    std::cout << "device.name after reopen = "
              << reopenedConfig.getString("device.name", "default") << std::endl;
    std::cout << "startup.delay after reopen = "
              << reopenedConfig.getInt("startup.delay", 0) << std::endl;
    std::cout << "logging.enabled after reopen = "
              << (reopenedConfig.getBool("logging.enabled", false) ? "true" : "false")
              << std::endl;
    reopenedConfig.close();

    std::cout << "Storage demo complete." << std::endl;
    return 0;
}
