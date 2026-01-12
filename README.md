# supla-device

`supla-device` is an open-source **IoT device library / SDK** for building **custom SUPLA-compatible devices**.

It is used by **DIY enthusiasts, hardware manufacturers and advanced developers** to create devices that connect to the **SUPLA smart home platform**.

This repository provides building blocks for device firmware.  
It is **not a ready-to-flash firmware**.

---

## What is supla-device

`supla-device` is a software foundation for building custom smart home devices that work with SUPLA.

Typical use cases include devices such as:
- switches, gates and roller shutters,
- sensors (temperature, humidity, energy, etc.),
- **HVAC and thermostat devices**,
- devices combining multiple functions.

Devices built with `supla-device` communicate with **SUPLA Cloud** or a **self-hosted SUPLA server**.

At its core, the library is built around two key concepts.

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

`supla-device` is a good fit if you are:
- a **DIY or hobbyist** working with Arduino Mega, ESP32, or ESP8266,
- building your own smart home devices,
- a **hardware manufacturer** developing SUPLA-compatible products,
- someone willing to write and modify device firmware.

You do **not** need deep embedded knowledge to get started.  
Many users begin with simple Arduino-based projects and learn more advanced concepts over time.

---

### Note for hardware manufacturers

We strongly encourage **self-prototyping** using this repository.

For **commercial products**, we recommend **contacting the SUPLA team**:
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
- you want a complete smart home system (cloud, server, UI),
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

- **Online documentation (API & concepts)**  
  https://supla.github.io/supla-device/

- **Quick orientation (in-repo)**  
  See the `docs/` directory

- **Documentation sources and build scripts**  
  `extras/docs/`

SUPLA platform overview:  
https://www.supla.org/

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

## License

This project is licensed under the **GNU GPL v2**.  
See the [LICENSE](LICENSE) file for details.
