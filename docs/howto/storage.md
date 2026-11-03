# Storage

**supla-device** uses persistent storage for **configuration**
and selected runtime **state**.

---

## Overview

supla-device uses persistent storage to keep selected data between restarts.

There are two distinct categories of stored data:
- **configuration storage**,
- **state storage**.

These categories serve different purposes and have different lifecycle
and update characteristics.

---

## Configuration storage

**Configuration storage** is used to store device configuration.

Typical configuration data includes:
- network configuration (SSID, password, Ethernet settings),
- SUPLA connection details (server address),
- device credentials (email, GUID, AUTHKEY),
- Channel configuration (Type-related parameters, selected Function).

Configuration storage:
- is loaded during device startup,
- is written infrequently,
- is modified via SUPLA Cloud or local configuration mechanisms,
- must remain consistent across firmware updates.

Most devices require configuration storage.

Note: on ESP-IDF, sensitive device identity/credentials (such as GUID and AUTHKEY)
may be stored in a **separate partition** (or secure storage) instead of the main
configuration storage, depending on the device design and provisioning process.

---

### LittleFS mount lifetime

`Supla::LittleFsConfig` reuses the shared LittleFS mount. Configuration operations
and destruction of a Config object leave the filesystem mounted; each file and
directory iterator is still closed after use. Multiple Config objects and
application code can therefore use the mount without Config unmounting it.
This does not add synchronization for concurrent filesystem access.

The SDK detects the backend's mounted state instead of caching its own flag.
After application code explicitly calls `LittleFS.end()`, the next operation
attempts to mount again. Avoid unmounting or formatting while files or iterators
are in use. Ordinary operation failures leave the filesystem mounted; an
application that needs an explicit remount must arrange it when access is idle.
Formatting on mount failure is allowed only during explicit Config `init()`.
Compared with the previous backend, ordinary Config operations now return an
error instead of formatting: this includes blob access, commit, certificate CA
access for SUPLA/MQTT and removal. A failed operation cannot erase the partition
as an attempted recovery. An explicit filesystem format is an application
maintenance operation.

Keeping the mount retains its caches in heap between operations. In ESP8266
Arduino core 3.1.2 the mount uses three 64-byte buffers (192 bytes plus allocator
overhead); per-file buffers are separate and released when files close. This
is a source-derived allocation count, not a physical free-heap measurement.
Config write buffers and persisted formats are unchanged. Files are closed on
write completion; leaving the mount active does not defer Config commits until
unmount.

---

## State storage

**State storage** is used to store runtime state that should survive restarts.

Typical state data includes:
- impulse counter values,
- roller shutter positions,
- calibration or accumulated measurements.

State storage:
- is updated during normal device operation,
- may be written periodically,
- is not required by all Elements.

Some Elements function correctly without state storage,
others require it to behave properly.

---

### Write frequency and flash wear (important)

State storage is often backed by **Flash memory**.

Important considerations:
- Flash memory has a **limited number of write/erase cycles**.
- Storage implementations may **not always provide wear-leveling**.
- Even when wear-leveling is present, excessive writes significantly
  reduce memory lifetime.

Implications:
- state should not be written on every change,
- Elements should rely on the storage layer to throttle writes,
- frequent or unnecessary state updates should be avoided.

Design Elements with the assumption that:
- writes are relatively expensive,
- persistence should be used only when required.

---

## Configuration vs state (important distinction)

It is important to clearly distinguish between configuration and state:

- **Configuration** defines how the device should behave.
- **State** represents what the device was doing at the time of shutdown.

In some legacy cases, parts of Element configuration may also be stored
together with state data.

---

## Storage backends

The actual storage backend depends on platform and hardware.

Common backends include:
- internal Flash memory,
- EEPROM,
- external non-volatile memory (e.g. FRAM),
- file-based storage (Linux / sd4linux).

supla-device abstracts storage access so that Element logic
remains portable across platforms.

On Linux / sd4linux, storage is typically implemented using
file-based backends provided by the operating system.
Configuration and state may be stored in regular files,
depending on the selected implementation.

---

## Storage lifecycle

During device startup:
- configuration storage is read first,
- state storage is read next (if present).

During runtime:
- configuration is updated only when changed,
- state is saved periodically as needed by Elements.

Storage write frequency is controlled by the storage implementation
to avoid excessive wear of non-volatile memory.

See also:
- [Concepts: Element lifecycle](../concepts/element-lifecycle.md) – `onSaveState` callback and write frequency considerations.

---

## Implementation overview

Core storage abstractions are implemented in:

- `src/supla/storage/`

Platform-specific implementations is located in:
- `extras/porting/` (for non-Arduino lib platforms).

Elements interact with storage through abstractions,
not directly with hardware or filesystems.

