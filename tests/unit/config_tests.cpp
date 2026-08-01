#include "elap/config/KeyValueConfiguration.hpp"

#include <cassert>
#include <fstream>
#include <string>

namespace {

void runConfigTests()
{
    const std::string path = "/tmp/elap_config_tests.conf";
    {
        std::ofstream file(path);
        file << "# comment\n";
        file << "service.name = sample_service\n";
        file << "worker.count= 3\n";
        file << "feature.enabled=yes\n";
        file << "invalid.int=12x\n";
    }

    elap::config::KeyValueConfiguration configuration;
    std::string error;
    assert(configuration.loadFromFile(path, &error));
    assert(configuration.has("service.name"));
    assert(configuration.getString("service.name", "") == "sample_service");
    assert(configuration.getString("missing", "default") == "default");
    assert(configuration.getInt("worker.count", 1) == 3);
    assert(configuration.getInt("invalid.int", 7) == 7);
    assert(configuration.getBool("feature.enabled", false));
    assert(!configuration.getBool("missing.bool", false));

    elap::config::KeyValueConfiguration invalid;
    {
        std::ofstream file(path);
        file << "not valid\n";
    }
    assert(!invalid.loadFromFile(path, &error));
    assert(!error.empty());

    assert(!configuration.loadFromFile(path, &error));
    assert(configuration.has("service.name"));
    assert(configuration.getString("service.name", "") == "sample_service");
}

} // namespace

int main();

struct ConfigTestRunner {
    ConfigTestRunner()
    {
        runConfigTests();
    }
} configTestRunner;
