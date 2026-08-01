#include "elap/health/HealthMonitor.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>
#include <sys/statvfs.h>
#include <utility>

namespace elap::health {
namespace {

void appendError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage == nullptr) {
        return;
    }
    if (!errorMessage->empty()) {
        *errorMessage += "; ";
    }
    *errorMessage += message;
}

bool isHealthy(service::ServiceState state)
{
    return state == service::ServiceState::Initialized
        || state == service::ServiceState::Starting
        || state == service::ServiceState::Running;
}

} // namespace

std::uint64_t CpuSample::total() const
{
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

std::uint64_t CpuSample::idleTotal() const
{
    return idle + iowait;
}

double MemoryInfo::usedPercent() const
{
    if (totalKb == 0 || availableKb > totalKb) {
        return 0.0;
    }
    return static_cast<double>(totalKb - availableKb) * 100.0 / static_cast<double>(totalKb);
}

double DiskInfo::usedPercent() const
{
    if (totalBytes == 0 || availableBytes > totalBytes) {
        return 0.0;
    }
    return static_cast<double>(totalBytes - availableBytes) * 100.0
        / static_cast<double>(totalBytes);
}

void HealthMonitor::setProcRoot(std::string procRoot)
{
    const std::lock_guard lock(mutex_);
    procRoot_ = std::move(procRoot);
}

void HealthMonitor::setDiskPath(std::string diskPath)
{
    const std::lock_guard lock(mutex_);
    diskPath_ = std::move(diskPath);
}

void HealthMonitor::reportService(const std::string& name, service::ServiceState state)
{
    const std::lock_guard lock(mutex_);
    const auto existing = std::find_if(services_.begin(), services_.end(),
                                       [&name](const ServiceHealth& health) {
                                           return health.name == name;
                                       });
    const ServiceHealth health {name, state, isHealthy(state)};
    if (existing == services_.end()) {
        services_.push_back(health);
    } else {
        *existing = health;
    }
}

void HealthMonitor::clearServices()
{
    const std::lock_guard lock(mutex_);
    services_.clear();
}

HealthSnapshot HealthMonitor::collect(std::string* errorMessage) const
{
    HealthSnapshot snapshot;
    std::string procRoot;
    std::string diskPath;
    {
        const std::lock_guard lock(mutex_);
        snapshot.services = services_;
        procRoot = procRoot_;
        diskPath = diskPath_;
    }

    {
        std::ifstream stat(procRoot + "/stat");
        std::string line;
        if (stat && std::getline(stat, line)) {
            snapshot.cpu = parseCpuStatLine(line);
        }
        if (!snapshot.cpu.has_value()) {
            appendError(errorMessage, "failed to read cpu health");
        }
    }

    {
        std::ifstream meminfo(procRoot + "/meminfo");
        std::ostringstream buffer;
        if (meminfo) {
            buffer << meminfo.rdbuf();
            snapshot.memory = parseMemInfo(buffer.str());
        }
        if (!snapshot.memory.has_value()) {
            appendError(errorMessage, "failed to read memory health");
        }
    }

    struct statvfs stats {};
    if (::statvfs(diskPath.c_str(), &stats) == 0) {
        snapshot.disk = DiskInfo {
            static_cast<std::uint64_t>(stats.f_blocks) * stats.f_frsize,
            static_cast<std::uint64_t>(stats.f_bavail) * stats.f_frsize
        };
    } else {
        appendError(errorMessage, "failed to read disk health");
    }

    return snapshot;
}

std::optional<CpuSample> HealthMonitor::parseCpuStatLine(const std::string& line)
{
    std::istringstream stream(line);
    std::string label;
    CpuSample sample;
    stream >> label >> sample.user >> sample.nice >> sample.system >> sample.idle
        >> sample.iowait >> sample.irq >> sample.softirq >> sample.steal;
    if (!stream || label != "cpu") {
        return std::nullopt;
    }
    return sample;
}

std::optional<MemoryInfo> HealthMonitor::parseMemInfo(const std::string& text)
{
    std::istringstream stream(text);
    std::string key;
    std::uint64_t value = 0;
    std::string unit;
    MemoryInfo info;
    bool foundTotal = false;
    bool foundAvailable = false;

    while (stream >> key >> value >> unit) {
        if (key == "MemTotal:") {
            info.totalKb = value;
            foundTotal = true;
        } else if (key == "MemAvailable:") {
            info.availableKb = value;
            foundAvailable = true;
        }
    }

    if (!foundTotal || !foundAvailable || info.totalKb == 0) {
        return std::nullopt;
    }
    return info;
}

} // namespace elap::health
