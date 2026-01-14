# supla-device – codebase orientation

This file provides high-level orientation for understanding the
**supla-device** codebase.

It describes core concepts, architectural boundaries and where
to look for specific functionality.

---

## Project scope

**supla-device** is a device-side SDK for building SUPLA-compatible devices.

It:
- owns device runtime, networking and protocol handling,
- runs on Arduino IDE, PlatformIO, ESP-IDF and Linux,
- is not a ready-made firmware.

Device code integrates with supla-device instead of replacing its core logic.

---

## Core concepts

### Elements
- Implement internal device logic.
- Execute lifecycle callbacks during runtime.
- May expose Channels, but do not have to.

Base class:
- `src/supla/element.h`

---

### Channels
- Represent user-visible functionality.
- Are registered during device startup.
- Are linked to Elements.

Implementation:
- `src/supla/channels/`

---

### Channel Type vs Function
- **Type**: defined in firmware, validated during registration, must remain stable.
- **Function**: selected in SUPLA Cloud, not validated during registration.

Channel numbering is defined in firmware.
Dynamic devices often use explicit numbering to preserve identifiers.

---

## Networking

supla-device **manages networking**:
- configuration,
- interface setup,
- connection and reconnection,
- communication with SUPLA Server.

Device code should not manage networking independently.

Server ports:
- **2016** – TLS (default),
- **2015** – unencrypted (deprecated).

---

## Storage

Two storage types:
- **Configuration storage** – credentials, network config, Channel config.
- **State storage** – runtime state (counters, positions).

Avoid frequent writes.
Flash-backed storage may have limited or no wear-leveling.

On ESP-IDF, GUID and AUTHKEY may be stored in a dedicated partition.

---

## GPIO abstraction (HAL)

GPIO access is abstracted.

Use the GPIO HAL instead of direct `pinMode`, `digitalRead`, `digitalWrite`.

Relevant code:
- `src/supla/io.h`
- `src/supla/io/`

---

## Logging

Unified logging via macros:
- `SUPLA_LOG_DEBUG`
- `SUPLA_LOG_INFO`
- `SUPLA_LOG_WARNING`
- `SUPLA_LOG_ERROR`

`printf`-style API.
Works across Arduino, ESP-IDF and Linux.
Can be disabled at compile time.

---

## Code layout

Core code:
- `src/SuplaDevice.h`, `src/SuplaDevice.cpp` - main entry points
- `src/supla/` - framework core
- `src/supla-common/` - shared protocol definitions

Key submodules:
- `channels/`, `network/`, `storage/`, `protocol/`, `device/`,
  `control/`, `sensor/`, `pv/`

---

## Constraints and pitfalls

- Do not manage networking outside supla-device.
- Do not change Channel Type for registered devices.
- Use explicit Channel numbering for dynamic devices.
- Avoid frequent state writes.
- Do not mix direct GPIO access with the GPIO HAL.

---

## Summary

supla-device is a multi-platform device SDK that:
- owns networking and protocol handling,
- separates internal logic (Elements) from exposed functionality (Channels),
- abstracts hardware access and logging.

Understanding these boundaries is essential when working with the codebase.

