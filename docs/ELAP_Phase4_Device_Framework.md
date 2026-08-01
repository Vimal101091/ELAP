# ELAP Phase 4 Device Framework

## Scope

Phase 4 introduces a small device framework for Linux-backed embedded
devices. The first implementation focuses on lifecycle management and
testable host abstractions that can be reused by UART, SPI, I2C, GPIO,
CAN, and USB adapters.

## Components

### IDevice

`elap::device::IDevice` defines the common lifecycle:

- `open()`
- `close()`
- `state()`
- `name()`

The framework tracks devices as `Closed`, `Open`, or `Failed`.

### DeviceManager

`elap::device::DeviceManager` owns named devices and can open or close
all registered devices together. Duplicate names are rejected so services
can safely look up devices by logical name.

### FileDevice

`elap::device::FileDevice` is an RAII wrapper around a Linux file
descriptor. It is suitable for character-device style endpoints such as
UARTs, SPI/I2C device nodes, USB serial adapters, and test loopback
files.

Responsibilities:

- Open a configured path with caller-supplied flags
- Close on destruction
- Read and write byte buffers
- Expose the native file descriptor for advanced Linux calls

### GpioPin

`elap::device::GpioPin` provides sysfs-style GPIO access. The GPIO root
is configurable, so tests can use a temporary directory instead of real
hardware.

Responsibilities:

- Export a pin when needed
- Configure direction
- Read and write pin values
- Unexport on close

## Current Tests

Unit coverage validates:

- File-device open, read, write, and close
- DeviceManager registration, duplicate-name rejection, lookup, and
  open/close lifecycle
- GPIO direction and value behavior using a temporary sysfs-style tree

## Next Activities

Recommended next activities:

1. Add UART configuration helpers for baud rate, parity, and flow control
2. Add SPI transfer support using `ioctl(SPI_IOC_MESSAGE)`
3. Add I2C read/write helpers using Linux i2c-dev
4. Add CAN socket support using SocketCAN
5. Add device configuration loading from `DatabaseConfiguration`
