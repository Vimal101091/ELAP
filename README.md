# Embedded Linux Application Platform (ELAP)

ELAP is a reusable C++20 runtime foundation for embedded Linux
applications. The current implementation covers the Phase 1 core
runtime, Phase 2 IPC primitives, Phase 3 SQLite-backed persistent
storage, Phase 4 device framework foundations, and Phase 5 health
monitoring foundations.

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
- POSIX message queue wrapper
- Shared memory wrapper
- Message-queue-backed event bus
- Length-prefixed IPC message framing
- Separate-process IPC echo server/client sample
- Service-framework IPC server plus client sample
- Process-level samples for POSIX message queues, shared memory, and the
  event bus
- Unit coverage for Unix sockets, message queues, shared memory, and
  event bus behavior

Phase 3 Storage support:

- SQLite RAII wrapper with parameterized queries
- Persistent key-value/blob store backed by SQLite
- Atomic batch writes for database seeding
- Database-backed `IConfiguration` implementation
- Sample storage demo and config service executables
- Unit and integration test coverage for storage modules

Phase 4 Device Framework support:

- Common `IDevice` lifecycle and state model
- Device registry/manager for named devices
- RAII file-device wrapper for Linux character-device endpoints
- Sysfs-style GPIO pin wrapper with configurable root for tests
- Unit coverage for file devices, manager behavior, and GPIO access

Phase 5 Health Monitoring support:

- CPU counter collection from proc-style `stat`
- Memory collection from proc-style `meminfo`
- Disk capacity collection with `statvfs`
- Service health tracking from `ServiceState`
- Unit coverage for parsers, snapshot collection, and service state
  health derivation

Later phases will add production logging backends, plugins, security,
OTA, supervision, and Yocto integration.

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
- `build/ipc_echo_server`
- `build/ipc_echo_client`
- `build/sample_ipc_server`
- `build/sample_ipc_client`
- `build/ipc_mq_receiver`
- `build/ipc_mq_sender`
- `build/ipc_shm_writer`
- `build/ipc_shm_reader`
- `build/ipc_event_receiver`
- `build/ipc_event_publisher`
- `build/storage_demo`
- `build/storage_config_service`
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

The CTest suite includes unit tests, sample service integration tests,
a separate-process IPC echo round-trip test, and a service-framework IPC
server/client round-trip test. It also validates process-level POSIX
message queue, shared memory, and event bus round trips, plus a storage
demo round trip.

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

## Run Sample IPC Apps

Start the IPC server service:

```bash
./build/sample_ipc_server --config ./config/sample_ipc_server.conf
```

In another terminal, call it with the client:

```bash
./build/sample_ipc_client --socket /tmp/elap_sample_ipc_server.sock --message status
```

Expected response:

```text
sample_ipc_server:status
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
- [Phase 3 storage design](docs/ELAP_Phase3_Storage.md)
- [Phase 4 device framework](docs/ELAP_Phase4_Device_Framework.md)
- [Phase 5 health monitoring](docs/ELAP_Phase5_Health_Monitoring.md)
- [Periodic sensor read flow](docs/PeriodicSensorReadFlow.md)

## Current Phase 1 Status

The current implementation covers the core Phase 1 runtime and sample
service. It also includes an automated integration test for sample
service graceful shutdown.

## Current Phase 2 Status

Phase 2 has Unix domain socket IPC, POSIX message queue, shared memory,
and event bus support. Each IPC primitive has unit coverage and
process-level integration coverage. The next recommended activity is to
decide whether the event bus should support fan-out delivery, topic
filtering, or multiple backend transports.

## Current Phase 3 Status

Phase 3 has SQLite-based persistent storage with three layers: a
low-level RAII database wrapper, a schema-managed key-value store, and a
database-backed configuration implementation. Configuration seeding now
parses and validates the full seed file before committing changes, so
invalid files do not partially update the database. Store read paths also
propagate lower-level query errors through the existing error-message
out-parameter. The SQLite source is bundled as an amalgamation under
`third_party/sqlite3/` so no external development package is required.

Current verification status: `cmake --build build` and
`ctest --output-on-failure` pass with 9/9 tests.

## Current Phase 4 Status

Phase 4 has a common device lifecycle, a named device manager, a
file-descriptor-backed device wrapper, and a sysfs-style GPIO wrapper.
This is the foundation for UART, SPI, I2C, CAN, and USB adapters.

## Current Phase 5 Status

Phase 5 has a local health monitor that collects CPU, memory, disk, and
service-state health snapshots. The collector accepts configurable proc
and disk paths so it can be tested without relying on host-specific
values.
