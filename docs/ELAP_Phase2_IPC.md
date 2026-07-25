# ELAP Phase 2 IPC Design

## Scope

Phase 2 introduces inter-process communication primitives that services
can use to exchange commands, events, and data on an embedded Linux host.

Planned Phase 2 components:

- Unix domain sockets
- Shared memory
- POSIX message queues
- Event bus

## Implementation

Phase 2 IPC support lives under `elap::ipc`.

Implemented classes:

- `UnixSocketServer`
- `UnixSocketClient`
- `UnixSocketConnection`
- `PosixMessageQueue`
- `SharedMemoryRegion`
- `MessageQueueEventBus`

The Unix socket API uses stream sockets with length-prefixed messages.
This hides partial read/write handling from service code while keeping
the transport simple and Linux-native.

The POSIX message queue API supports named queue creation/opening,
priority send, receive, and queue cleanup. It is intended for small
asynchronous commands and notifications.

The shared memory API supports named region creation/opening, mapping,
bounded read/write, and cleanup. It is intended for larger payloads where
copying through sockets or queues would be wasteful.

The event bus is currently backed by POSIX message queues. It encodes
events as a topic plus payload and provides publish/receive operations.

Sample executables:

- `ipc_echo_server`
- `ipc_echo_client`
- `sample_ipc_server`
- `sample_ipc_client`

The echo sample proves the IPC wrapper works across separate Linux
processes. The server listens on a Unix domain socket, accepts one
client, receives one framed message, replies with an `echo:` prefix, and
exits.

The sample IPC server/client pair proves the same IPC mechanism inside
the ELAP service framework. `sample_ipc_server` runs as a long-lived
service, starts an IPC listener thread, handles framed requests, and
shuts down cleanly on `SIGTERM` or `SIGINT`. `sample_ipc_client` sends a
single request and prints the framed reply.

## Message Framing

Each message is encoded as:

```text
uint32_be payload_size
payload bytes
```

The receiver validates payload size against a caller-provided maximum
before allocating storage for the payload.

## Current Tests

Unit coverage validates:

- Server listen setup
- Client connect
- Client-to-server message send
- Server-to-client reply
- Closed connection error handling
- Receive size-limit enforcement
- POSIX message queue create/open/send/receive
- POSIX message queue priority preservation
- POSIX message queue size-limit rejection
- Shared memory create/open/read/write
- Shared memory bounds checking
- Event bus publish/receive
- Event topic validation

Integration coverage validates:

- Starting the echo server as a separate process
- Connecting with the echo client as a separate process
- Exchanging a framed request/reply message across the Unix socket
- Clean server exit after the request is handled
- Starting the sample IPC server as an ELAP service
- Calling it from the sample IPC client
- Graceful shutdown of the IPC listener thread

Unix domain socket bind/listen and POSIX IPC objects may require running
tests outside heavily restricted sandboxes.

## Next Phase 2 Activities

Recommended next activities:

1. Add process-level samples or integration tests for POSIX message
   queues.
2. Add process-level samples or integration tests for shared memory.
3. Add process-level samples or integration tests for the event bus.
4. Decide whether the event bus should support fan-out delivery, topic
   filtering, or multiple backend transports.
