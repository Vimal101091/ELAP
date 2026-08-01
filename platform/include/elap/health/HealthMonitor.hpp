#pragma once

#include "elap/service/ServiceState.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace elap::health {

struct CpuSample {
    std::uint64_t user {0};
    std::uint64_t nice {0};
    std::uint64_t system {0};
    std::uint64_t idle {0};
    std::uint64_t iowait {0};
    std::uint64_t irq {0};
    std::uint64_t softirq {0};
    std::uint64_t steal {0};

    std::uint64_t total() const;
    std::uint64_t idleTotal() const;
};

struct MemoryInfo {
    std::uint64_t totalKb {0};
    std::uint64_t availableKb {0};

    double usedPercent() const;
};

struct DiskInfo {
    std::uint64_t totalBytes {0};
    std::uint64_t availableBytes {0};

    double usedPercent() const;
};

struct ServiceHealth {
    std::string name;
    service::ServiceState state {service::ServiceState::Created};
    bool healthy {false};
};

struct HealthSnapshot {
    std::optional<CpuSample> cpu;
    std::optional<MemoryInfo> memory;
    std::optional<DiskInfo> disk;
    std::vector<ServiceHealth> services;
};

class HealthMonitor {
public:
    void setProcRoot(std::string procRoot);
    void setDiskPath(std::string diskPath);

    void reportService(const std::string& name, service::ServiceState state);
    void clearServices();

    HealthSnapshot collect(std::string* errorMessage = nullptr) const;

    static std::optional<CpuSample> parseCpuStatLine(const std::string& line);
    static std::optional<MemoryInfo> parseMemInfo(const std::string& text);

private:
    std::string procRoot_ {"/proc"};
    std::string diskPath_ {"/"};
    std::vector<ServiceHealth> services_;
    mutable std::mutex mutex_;
};

} // namespace elap::health
