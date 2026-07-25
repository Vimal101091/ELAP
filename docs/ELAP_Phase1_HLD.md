# ELAP Phase 1 High-Level Design

## Version

1.0

## Scope

Phase 1 establishes the core runtime foundation for the Embedded Linux
Application Platform. The implementation should provide a small,
reusable framework that all later ELAP services and customer
applications can build on.

Phase 1 includes:

-   Service Framework
-   Logger
-   Configuration Manager
-   Signal Handling
-   Thread Manager

Phase 1 does not include full IPC, persistent database storage, device
drivers, OTA, security, plugin loading, or Yocto image integration.
Those are introduced in later phases.

## Design Goals

-   Provide a predictable service lifecycle
-   Support graceful startup and shutdown
-   Provide common logging from the beginning
-   Load runtime configuration from files
-   Manage worker threads safely
-   Keep APIs simple enough for later platform services to reuse
-   Make the core testable on a normal Linux development machine

## High-Level Architecture

``` text
Application / Platform Service
        |
        v
Service Framework
        |
        +--> Logger
        |
        +--> Configuration Manager
        |
        +--> Signal Handler
        |
        +--> Thread Manager
        |
        v
Linux / POSIX Runtime
```

The service framework owns the top-level lifecycle. Supporting modules
provide cross-cutting runtime capabilities.

## Core Components

### Service Framework

The service framework provides the base structure for all ELAP services.
Each service should follow the same lifecycle so that startup, runtime,
and shutdown behavior are consistent.

Responsibilities:

-   Define a common service interface
-   Initialize platform modules in the correct order
-   Start service-specific logic
-   Run until a shutdown request is received
-   Stop worker threads and release resources cleanly
-   Return meaningful process exit codes

Proposed lifecycle:

``` text
create
  |
  v
initialize
  |
  v
start
  |
  v
run
  |
  v
stop
  |
  v
deinitialize
```

Proposed service states:

-   Created
-   Initializing
-   Initialized
-   Starting
-   Running
-   Stopping
-   Stopped
-   Failed

Suggested interface:

``` cpp
class IService {
public:
    virtual ~IService() = default;

    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void deinitialize() = 0;
    virtual const char* name() const = 0;
};
```

Suggested runtime owner:

``` cpp
class ServiceApplication {
public:
    int run(IService& service, int argc, char** argv);
};
```

### Logger

The logger provides a simple platform-wide logging API. In Phase 1, the
logger should support console output and basic severity filtering. File
logging and journald can be expanded in Phase 6.

Responsibilities:

-   Provide a common logging API
-   Support severity levels
-   Include timestamp, level, component, and message
-   Be safe to call from multiple threads
-   Allow log level to be configured

Severity levels:

-   Trace
-   Debug
-   Info
-   Warning
-   Error
-   Critical

Example output:

``` text
2026-07-19T10:15:30.123Z INFO service_manager Service started
```

Suggested interface:

``` cpp
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level,
                     const char* component,
                     const char* message) = 0;
};
```

### Configuration Manager

The configuration manager loads service configuration during startup.
Phase 1 should use a simple file-based configuration format. A key-value
format is enough for the first implementation; JSON or TOML can be added
later if needed.

Responsibilities:

-   Load configuration from a file path
-   Provide typed accessors for common values
-   Provide default values when keys are missing
-   Report invalid configuration clearly
-   Avoid global mutable configuration state where possible

Example configuration:

``` text
service.name=sample_service
log.level=info
worker.count=2
shutdown.timeout_ms=5000
```

Suggested interface:

``` cpp
class IConfiguration {
public:
    virtual ~IConfiguration() = default;

    virtual bool has(const std::string& key) const = 0;
    virtual std::string getString(const std::string& key,
                                  const std::string& defaultValue) const = 0;
    virtual int getInt(const std::string& key, int defaultValue) const = 0;
    virtual bool getBool(const std::string& key, bool defaultValue) const = 0;
};
```

### Signal Handler

The signal handler converts Linux process signals into a controlled
shutdown request. Phase 1 should handle termination signals and avoid
doing complex work directly inside signal handlers.

Responsibilities:

-   Register handlers for SIGINT and SIGTERM
-   Expose a shutdown-requested flag or callback
-   Wake the service runtime from its run loop
-   Support graceful shutdown

Signals:

-   SIGINT: interactive stop, usually Ctrl+C
-   SIGTERM: normal process termination request

Design rule:

-   The low-level signal handler should only perform async-signal-safe
    operations.
-   Cleanup should happen in the normal service control path.

### Thread Manager

The thread manager provides a small wrapper around worker thread
creation, stop requests, and joining. It should make it harder for
services to leak threads or exit while work is still running.

Responsibilities:

-   Start named worker threads
-   Provide cooperative stop requests
-   Join all managed threads during shutdown
-   Log thread start and stop events
-   Prevent accidental detached thread usage in platform services

Suggested behavior:

-   A worker receives a stop token or shared stop flag.
-   Workers periodically check for stop requests.
-   Service shutdown requests stop for all workers.
-   Service shutdown joins all workers before deinitialization.

Suggested interface:

``` cpp
class ThreadManager {
public:
    using Worker = std::function<void(std::stop_token)>;

    bool startThread(const std::string& name, Worker worker);
    void requestStop();
    void joinAll();
};
```

## Startup Flow

``` text
main()
  |
  v
create concrete service
  |
  v
create ServiceApplication
  |
  v
parse command-line arguments
  |
  v
load configuration
  |
  v
initialize logger
  |
  v
register signal handlers
  |
  v
service.initialize()
  |
  v
service.start()
  |
  v
wait until shutdown requested
  |
  v
service.stop()
  |
  v
join worker threads
  |
  v
service.deinitialize()
  |
  v
exit
```

## Shutdown Flow

``` text
SIGINT / SIGTERM
  |
  v
signal handler records shutdown request
  |
  v
service run loop wakes up
  |
  v
service stop begins
  |
  v
thread manager requests worker stop
  |
  v
workers exit cooperatively
  |
  v
thread manager joins all workers
  |
  v
service resources released
  |
  v
process exits
```

## Proposed Repository Layout For Phase 1

``` text
ELAP/
|-- CMakeLists.txt
|-- docs/
|   |-- ELAP_Project_Strategy.md
|   `-- ELAP_Phase1_HLD.md
|-- common/
|   |-- include/
|   `-- src/
|-- platform/
|   |-- include/
|   |   `-- elap/
|   |       |-- service/
|   |       |-- logging/
|   |       |-- config/
|   |       |-- signals/
|   |       `-- threading/
|   `-- src/
|       |-- service/
|       |-- logging/
|       |-- config/
|       |-- signals/
|       `-- threading/
|-- applications/
|   `-- sample_service/
|-- tests/
|   |-- unit/
|   `-- integration/
`-- scripts/
```

The current workspace may start with the documents at the root. When
implementation begins, the documents can be moved under `docs/`.

## Build And Runtime Assumptions

-   Language: C++20
-   Build system: CMake
-   OS: Linux development host first, embedded Linux target later
-   Compiler: GCC or Clang
-   Unit test framework: GoogleTest
-   Initial runtime target: Ubuntu or similar Linux environment

## Error Handling Strategy

-   Initialization failures should be logged and should stop startup.
-   Runtime errors should be logged with enough component context.
-   Service lifecycle methods should avoid throwing exceptions across
    module boundaries.
-   Constructors should not perform heavy runtime initialization.
-   Public APIs should make ownership and failure behavior clear.

## Threading Strategy

-   Prefer `std::jthread` and `std::stop_token` where available.
-   Use mutexes only around shared data that must be protected.
-   Avoid global mutable state.
-   Avoid detached threads.
-   All worker threads must exit during service shutdown.

## Configuration Strategy

Default configuration path:

``` text
/etc/elap/<service-name>.conf
```

Development override:

``` text
./config/<service-name>.conf
```

Command-line option:

``` text
--config <path>
```

Phase 1 should support command-line override first. Later phases can add
schema validation, database-backed configuration, and secure
configuration handling.

## Logging Strategy

Phase 1 logger output targets:

-   Console stdout for Trace, Debug, Info, and Warning
-   Console stderr for Error and Critical

Later phases can add:

-   journald backend
-   File backend
-   Rotation
-   Remote log streaming

## Sample Service For Validation

Phase 1 should include a small sample service that proves the framework
works.

Sample service behavior:

-   Loads configuration
-   Starts one or more worker threads
-   Logs a heartbeat message periodically
-   Handles Ctrl+C cleanly
-   Stops all workers before exit

Example:

``` text
./sample_service --config ./config/sample_service.conf
```

Expected behavior:

-   Service starts successfully
-   Logs startup messages
-   Periodically logs heartbeat messages
-   On SIGINT, logs shutdown message
-   Joins workers
-   Exits with code 0

## Testing Strategy For Phase 1

Phase 1 testing should prove that the core runtime behaves correctly as
a user-space Linux application. The test strategy is split into unit
tests, integration tests, manual runtime tests, and basic reliability
checks.

### Test Environment

Primary test environment:

-   Ubuntu or equivalent Linux development machine
-   GCC or Clang with C++20 support
-   CMake
-   GoogleTest
-   POSIX signal and threading support

Optional test environments:

-   WSL2 for early development
-   QEMU-based Linux target for later embedded validation
-   Raspberry Pi or similar board after Phase 1 is stable

### Build Verification

The project should build from a clean directory using standard CMake
commands.

``` bash
cmake -S . -B build
cmake --build build
```

Expected result:

-   Platform library builds successfully
-   Sample service executable builds successfully
-   Unit test executables build successfully
-   No compiler warnings for newly added Phase 1 code, where practical

### Unit Tests

Unit tests should validate each module in isolation. Hardware,
networking, kernel drivers, and external services are not required for
Phase 1 unit testing.

Configuration manager tests:

-   Configuration parser loads valid files
-   Configuration parser handles missing keys
-   Default values are returned when keys are absent
-   Invalid integer and boolean values are reported clearly
-   Comments and blank lines are ignored
-   Missing configuration files fail gracefully

Logger tests:

-   Logger filters messages below the configured severity
-   Logger accepts all defined severity levels
-   Logger output includes timestamp, severity, component, and message
-   Logger can be called safely from multiple threads

Service framework tests:

-   Service lifecycle transitions are valid
-   Startup fails if initialization fails
-   Stop is called only after a successful start
-   Deinitialization is called during cleanup
-   Correct exit codes are returned for success and failure

Signal handler tests:

-   SIGINT requests shutdown
-   SIGTERM requests shutdown
-   Repeated shutdown requests do not corrupt state
-   Signal handling does not perform complex cleanup inside the signal
    handler itself

Thread manager tests:

-   Worker threads start successfully
-   Stop requests are propagated to workers
-   `joinAll()` waits for all workers to exit
-   Multiple workers can run and stop cleanly
-   No detached worker threads are used by the framework

Unit test command:

``` bash
ctest --test-dir build
```

### Integration Tests

Integration tests should run the sample service as a real process and
verify end-to-end behavior.

-   Sample service starts and stops cleanly
-   Sample service exits on SIGTERM
-   Invalid configuration prevents startup
-   Multiple worker threads stop cleanly
-   Startup logs are emitted
-   Heartbeat logs are emitted while running
-   Shutdown logs are emitted before process exit
-   Process exits with code 0 after graceful shutdown
-   Process exits with non-zero code for invalid configuration

Suggested automated integration flow:

``` text
start sample_service with valid config
wait for startup log
wait for heartbeat log
send SIGTERM
wait for process exit
verify exit code is 0
verify shutdown log exists
```

Suggested invalid configuration flow:

``` text
start sample_service with invalid config
verify startup fails
verify error log exists
verify process exits with non-zero code
```

### Manual Runtime Tests

Manual tests should be simple enough to run during development.

-   Run sample service from terminal
-   Press Ctrl+C and verify graceful shutdown
-   Send SIGTERM using `kill`
-   Confirm worker thread shutdown messages are printed
-   Confirm the shell receives exit code 0 after graceful shutdown
-   Run under valgrind or sanitizers when available

Example manual run:

``` bash
./build/applications/sample_service/sample_service --config ./config/sample_service.conf
```

Expected log sequence:

``` text
service starting
configuration loaded
worker thread started
heartbeat
shutdown requested
worker thread stopped
service stopped
```

Signal test:

``` bash
kill -TERM <sample-service-pid>
```

Expected result:

-   Service logs the shutdown request
-   Worker threads exit cooperatively
-   All workers are joined
-   Process exits cleanly

### Reliability Checks

Reliability checks are lightweight in Phase 1 but should catch obvious
resource management problems early.

-   Run the sample service for at least 10 minutes
-   Confirm heartbeat logging continues
-   Confirm memory usage remains stable
-   Start and stop the service repeatedly
-   Run with multiple worker counts
-   Run with sanitizers when enabled

Useful tools:

``` bash
valgrind ./build/applications/sample_service/sample_service --config ./config/sample_service.conf
```

``` bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan
```

### Phase 1 Test Completion Criteria

Testing for Phase 1 is complete when:

-   Clean CMake build succeeds
-   All unit tests pass
-   Sample service starts with a valid configuration
-   Sample service rejects invalid configuration
-   SIGINT triggers graceful shutdown
-   SIGTERM triggers graceful shutdown
-   All managed worker threads are joined before exit
-   No obvious leaks or thread lifetime issues are found in reliability
    checks
-   Test commands and expected results are documented in the developer
    README

## Phase 1 Deliverables

-   CMake project skeleton
-   Service framework interfaces and implementation
-   Console logger
-   File-based configuration manager
-   Signal handler
-   Thread manager
-   Sample service
-   Unit tests for core modules
-   Basic integration test for startup and shutdown
-   Developer README for building and running Phase 1

## Acceptance Criteria

Phase 1 is complete when:

-   The project builds using CMake
-   The sample service runs on a Linux development host
-   The sample service loads configuration from a file
-   The sample service logs startup, heartbeat, and shutdown messages
-   SIGINT and SIGTERM trigger graceful shutdown
-   All managed worker threads are joined before process exit
-   Unit tests pass
-   The design is ready for Phase 2 IPC integration

## Open Design Decisions

-   Whether configuration should start as key-value, JSON, or TOML
-   Whether the logger should expose macros, stream-style APIs, or plain
    function calls
-   Whether service lifecycle methods should return bool, error codes, or
    structured result objects
-   Whether to use `std::jthread` everywhere or provide a fallback for
    older toolchains
-   Whether Phase 1 should include a minimal command-line parser or use a
    small third-party library

## Recommended Implementation Order

1.  Create CMake project skeleton
2.  Add logger
3.  Add configuration manager
4.  Add signal handling
5.  Add thread manager
6.  Add service framework
7.  Add sample service
8.  Add unit tests
9.  Add integration test for graceful shutdown
10. Document build and run steps
