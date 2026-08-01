# ELAP Phase 5 Health Monitoring

## Scope

Phase 5 introduces local health monitoring for embedded Linux services.
The first implementation collects host-level CPU, memory, and disk
signals, and lets services report their own lifecycle state.

## Components

### HealthMonitor

`elap::health::HealthMonitor` collects a `HealthSnapshot` with:

- CPU counters from `/proc/stat`
- Memory totals from `/proc/meminfo`
- Disk capacity from `statvfs`
- Reported service health states

The proc root and disk path are configurable so tests and services can
use controlled inputs.

### CPU Health

`CpuSample` stores raw Linux CPU counters and exposes:

- `total()`
- `idleTotal()`

Keeping raw counters allows callers to compute utilization over two
samples without hiding timing policy inside the collector.

### Memory Health

`MemoryInfo` stores `MemTotal` and `MemAvailable` in KiB and provides a
`usedPercent()` helper.

### Disk Health

`DiskInfo` stores total and available bytes for a configured path and
provides a `usedPercent()` helper.

### Service Health

`ServiceHealth` records a service name, `ServiceState`, and derived
healthy flag. `Initialized`, `Starting`, and `Running` are currently
treated as healthy states.

## Current Tests

Unit coverage validates:

- CPU parser behavior
- Memory parser behavior
- Snapshot collection using temporary proc-style input files
- Disk collection from the local filesystem
- Service health status derivation

## Next Activities

Recommended next activities:

1. Add threshold evaluation and alert states
2. Publish health snapshots over the Phase 2 event bus
3. Persist health history using the Phase 3 storage layer
4. Add temperature collection from `/sys/class/thermal`
5. Add network interface counters from `/proc/net/dev`
