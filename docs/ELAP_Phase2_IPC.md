# ELAP Phase 2 IPC Design

## Scope

Phase 2 introduces inter-process communication primitives that services
can use to exchange commands, events, and data on an embedded Linux host.

Planned Phase 2 components:

- Unix domain sockets
- Shared memory
- POSIX message queues
- Event bus

## Initial Implementation

The first Phase 2 slice adds Unix domain socket support under
`elap::ipc`.

Implemented classes:

- `UnixSocketServer`
- `UnixSocketClient`
- `UnixSocketConnection`

The Unix socket API uses stream sockets with length-prefixed messages.
This hides partial read/write handling from service code while keeping
the transport simple and Linux-native.

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

Unix domain socket bind/listen may require running tests outside heavily
restricted sandboxes.

## Next Phase 2 Activities

Recommended next activities:

1. Add an IPC echo sample service or extend `sample_service` with an IPC
   control endpoint.
2. Add integration tests for IPC request/response across a real process
   boundary.
3. Add POSIX message queue wrapper for lightweight asynchronous
   commands.
4. Add shared memory support for larger data payloads.
5. Build the event bus abstraction on top of the selected IPC
   primitives.
