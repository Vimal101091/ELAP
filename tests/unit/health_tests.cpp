#include "elap/health/HealthMonitor.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::string testPath(const char* name)
{
    return "/tmp/elap_" + std::string(name) + "_" + std::to_string(::getpid());
}

void runCpuParserTest()
{
    const auto sample = elap::health::HealthMonitor::parseCpuStatLine(
        "cpu  100 2 30 400 5 6 7 8 0 0");
    assert(sample.has_value());
    assert(sample->user == 100);
    assert(sample->idleTotal() == 405);
    assert(sample->total() == 558);
    assert(!elap::health::HealthMonitor::parseCpuStatLine("intr 1 2 3").has_value());
}

void runMemoryParserTest()
{
    const auto info = elap::health::HealthMonitor::parseMemInfo(
        "MemTotal:       1000 kB\n"
        "MemFree:         200 kB\n"
        "MemAvailable:    750 kB\n");
    assert(info.has_value());
    assert(info->totalKb == 1000);
    assert(info->availableKb == 750);
    assert(info->usedPercent() == 25.0);
    assert(!elap::health::HealthMonitor::parseMemInfo("MemTotal: 1000 kB\n").has_value());
}

void runHealthCollectTest()
{
    const auto root = testPath("proc_root");
    std::filesystem::create_directories(root);
    {
        std::ofstream(root + "/stat") << "cpu  1 2 3 4 5 6 7 8 0 0\n";
        std::ofstream(root + "/meminfo")
            << "MemTotal:       4096 kB\n"
            << "MemAvailable:   1024 kB\n";
    }

    elap::health::HealthMonitor monitor;
    monitor.setProcRoot(root);
    monitor.setDiskPath("/");
    monitor.reportService("sample", elap::service::ServiceState::Running);
    monitor.reportService("failed", elap::service::ServiceState::Failed);

    std::string error;
    const auto snapshot = monitor.collect(&error);
    assert(snapshot.cpu.has_value());
    assert(snapshot.memory.has_value());
    assert(snapshot.disk.has_value());
    assert(snapshot.services.size() == 2);
    assert(snapshot.services[0].healthy);
    assert(!snapshot.services[1].healthy);
    assert(error.empty());

    std::filesystem::remove_all(root);
}

void runHealthTests()
{
    runCpuParserTest();
    runMemoryParserTest();
    runHealthCollectTest();
}

} // namespace

struct HealthTestRunner {
    HealthTestRunner()
    {
        runHealthTests();
    }
} healthTestRunner;
