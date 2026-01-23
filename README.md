<p align="center">
  <a href="https://www.supla.org/">
    <img src="docs/assets/supla-logo.svg" alt="SUPLA logo" width="180" />
  </a>
</p>

# supla-device

> 🤖 **For AI/LLM Assistants & Automated Tools:** Before making any changes, please analyze the primary instruction file: **[LLM.md](LLM.md)**

> Part of **SUPLA** — an open smart home platform that brings together hardware manufacturers, the community, and users.  
> Learn more at https://www.supla.org

`supla-device` is an open-source **IoT device library / SDK** for building **custom SUPLA-compatible devices**.

It provides building blocks for device firmware.
It is **not a ready-to-flash firmware**.

---

## What is supla-device

`supla-device` is a software foundation for implementing custom smart home devices that work with SUPLA.

Typical devices include:
- switches, gates, facade blinds and roller shutters,
- sensors (temperature, humidity, energy, binary on/off etc.),
- HVAC and thermostat devices,
- devices combining multiple functions.

Devices built with `supla-device` connect to **SUPLA Server** (cloud-hosted or self-hosted).

---

## Core concepts

### Channels

A **Channel** represents a device function that is **visible to the SUPLA server and user interfaces**.

Examples:
- a relay channel,
- a temperature sensor channel,
- a roller shutter channel,
- a thermostat or HVAC-related channel.

Channels define **what the user sees and controls** in SUPLA.

---

### Channels: Type vs Function

Each Channel has a **Type** and a configurable **Function**.

- Channel **Type** is defined in firmware and validated during device registration.
  It must remain stable.
- Channel **Function** is selected in SUPLA Cloud and can be changed
  without firmware updates.

Channel numbering is also defined in firmware.
For devices with dynamic Channels (e.g. gateways), explicit numbering
can be used to preserve stable Channel identifiers.

---

### Elements

An **Element** is an internal building block of the device software.

Elements:
- implement the actual device logic,
- provide lifecycle methods that are executed cyclically during program operation,
- may contain one or more Channels, **but do not have to**.

Many internal parts of a device are implemented as Elements **without any direct exposure to the SUPLA server**.

You can think of:
- **Channels** as *what the user sees*,
- **Elements** as *how the device works internally*.

---

## Who should use supla-device

`supla-device` is typically used by:
- **DIY or hobbyist** working with ESP32, ESP8266, or Arduino Mega, building your own smart home devices,
- hardware manufacturer developing SUPLA-compatible products,
- someone willing to write and modify device firmware.

Basic embedded experience is helpful, but not required.
Many users start with simple Arduino-based projects and expand from there.

---

### Note for hardware manufacturers

We strongly encourage **self-prototyping** using this repository.

For **commercial products**, we recommend **contacting the SUPLA team**
at [supla@supla.org](mailto:supla@supla.org):
- we can assist with device integration and architecture,
- we can help deliver **production-ready software**,
- we can support compliance with applicable regulations,  
  especially **RED (Radio Equipment Directive) and Delegated Acts (DA)**.

Software built directly from this repository **does not guarantee regulatory compliance** on its own.  
These requirements apply to **commercial products only**; DIY and experimental projects are not affected.

---

## Who should NOT use supla-device

`supla-device` may not be the best choice if:
- you are looking for a **ready-made firmware** to flash,
- you expect a **plug-and-play solution** without modifying code,
- you are not interested in working with device firmware at all.

---

## Supported platforms (overview)

`supla-device` supports several usage paths depending on your needs.

### Arduino library
- Available via **Arduino Library Manager**
- Works with Arduino IDE and PlatformIO
- Ideal for DIY projects and prototyping
- No prior embedded experience required
- You write and control your own device logic

When used on ESP32, the Arduino library relies on the same underlying
networking stack as ESP-IDF. From a networking and protocol perspective,
ESP32 behavior is equivalent across both environments.

### ESP32 / ESP-IDF
- Native integration
- Recommended for **commercial and long-term products**
- Full control over networking, memory and updates

### Linux (sd4linux)

`sd4linux` is a ready-to-run SUPLA device implementation for **Linux and macOS**.

It behaves like a SUPLA device and can be used for:
- integration with other systems,
- experimentation and rapid prototyping,
- debugging and development.

Integration options include:
- command-line tools,
- files,
- MQTT.

Linux support is intended for **integration and development**, not embedded hardware.

---

## Choose your path

If you are just starting:
- **Arduino library** → easiest entry point
- **ESP-IDF** → advanced or commercial devices
- **Linux (sd4linux)** → integration, testing and debugging

Most DIY users start with **Arduino library** and use **sd4linux** for other custom integrations.

See the `docs/` directory for a quick orientation.

---

## Documentation

- **Quick orientation (in-repo)**  
  See the [docs/index.md](docs/index.md) file in the `docs/` directory

- **Online documentation (API & concepts)**  
  https://supla.github.io/supla-device/

SUPLA platform overview:  
https://www.supla.org/

---

## Code orientation

Core code lives in `src/`.

### Entry points
- `src/SuplaDevice.h` / `src/SuplaDevice.cpp` – main user-facing entry point used in Arduino-style projects (`SuplaDevice.begin()`, `SuplaDevice.iterate()`, etc.)

### Core framework
- `src/supla/` – framework core: Elements, Channels, device runtime, protocol, networking and storage abstractions

Key files:
- `src/supla/element.h` – Element base class and lifecycle callbacks
- `src/supla/channels/` – Channel implementation (types, base classes, helpers)
- `src/supla/network/` – network interfaces and web-based configuration helpers
- `src/supla/storage/` – configuration storage and state storage backends (including optional wear-leveling)
- `src/supla/io.h` – GPIO Hardware Abstraction Layer (HAL). Provides a unified interface for GPIO access instead of direct `pinMode`, `digitalRead` and `digitalWrite` usage.
- `src/supla/io/` – concrete GPIO HAL implementations, including support for external I/O expanders.

Major feature areas:
- `src/supla/sensor/` – sensors and measurement-related Elements
- `src/supla/control/` – relays, buttons, roller shutters, HVAC and other control Elements
- `src/supla/protocol/` – SUPLA protocol layer and MQTT integration
- `src/supla/device/` – device-side services (registration, updates, status/diagnostics, conflict handling)
- `src/supla/pv/` – PV inverter integrations

### Protocol definitions shared across projects
- `src/supla-common/` – SUPLA protocol structures and low-level helpers shared across the ecosystem

---

### Logging

Logging is handled via platform-independent `SUPLA_LOG_*` macros
with `printf`-style formatting.
This allows unified logging across Arduino, ESP-IDF and Linux
and easy compile-time disabling for release builds.

---

## What supla-device is NOT

- Not a complete SUPLA system (no cloud, no server).
- Not a ready-made firmware.
- Not a generic hardware abstraction layer.
- Not guaranteed to preserve all internal APIs between releases.

---

## Project status

`supla-device` is actively developed as part of the SUPLA ecosystem.

Releases may introduce breaking changes required by protocol, platform or architectural evolution.  
See **Releases** and **[CHANGELOG](CHANGELOG.md)** for details.

---

## About SUPLA

For an overview of the SUPLA platform and ecosystem, see:
- [github.com/SUPLA](https://github.com/SUPLA)
- [www.supla.org](https://www.supla.org)

---

## License

This project is licensed under the **GNU GPL v2**.  
See the [LICENSE](LICENSE) file for details.

---
