# Embedded Linux Application Platform (ELAP)

ELAP is a reusable C++20 runtime foundation for embedded Linux
applications. Phase 1 focuses on the core platform pieces needed to run
long-lived user-space services with predictable startup, logging,
configuration, worker thread management, and graceful shutdown.

## Phase 1 Scope

Implemented Phase 1 components:

- Service framework with a common `IService` lifecycle
- Console logger with severity filtering
- File-based key-value configuration manager
- POSIX signal handling for graceful shutdown
- Thread manager with cooperative stop tokens
- Sample service that emits periodic heartbeat logs
- Unit tests for core modules

Initial Phase 2 IPC support:

- Unix domain socket server/client wrappers
- Length-prefixed IPC message framing
- Unit coverage for Unix socket request/reply behavior

Later phases will expand IPC and add storage, device framework support,
health monitoring, production logging backends, plugins, security, OTA,
supervision, and Yocto integration.

## Requirements

- Linux development host
- CMake 3.16 or newer
- C++20-capable compiler such as GCC or Clang
- POSIX threads

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

This builds:

- `build/libelap_platform.a`
- `build/sample_service`
- `build/elap_unit_tests`

## Run Tests

```bash
cd build
ctest --output-on-failure
```

You can also run the test executable directly:

```bash
./build/elap_unit_tests
```

The CTest suite includes unit tests and sample service integration tests
for graceful shutdown and invalid configuration handling.

## Run Sample Service

The sample service loads `./config/sample_service.conf` by default:

```bash
./build/sample_service
```

To pass a specific configuration file:

```bash
./build/sample_service --config ./config/sample_service.conf
```

Expected behavior:

- The service loads configuration
- Worker threads start
- Heartbeat messages are logged periodically
- `Ctrl+C` triggers graceful shutdown
- Worker threads are stopped and joined before exit

For a bounded manual smoke test:

```bash
timeout --signal=INT 5s ./build/sample_service
```

## Configuration

Default sample configuration:

```text
service.name=sample_service
log.level=info
worker.count=2
heartbeat.interval_ms=1000
shutdown.timeout_ms=5000
```

`worker.count` controls how many heartbeat worker threads are started.
`heartbeat.interval_ms` controls the delay between heartbeat log
messages.

## Documentation

- [Project strategy](docs/ELAP_Project_Strategy.md)
- [Phase 1 high-level design](docs/ELAP_Phase1_HLD.md)
- [Phase 2 IPC design](docs/ELAP_Phase2_IPC.md)
- [Periodic sensor read flow](docs/PeriodicSensorReadFlow.md)

## Current Phase 1 Status

The current implementation covers the core Phase 1 runtime and sample
service. It also includes an automated integration test for sample
service graceful shutdown.

## Current Phase 2 Status

Phase 2 has started with Unix domain socket IPC support. The next
recommended activity is to add a real process-level IPC integration test
or sample endpoint before adding POSIX message queues and shared memory.
