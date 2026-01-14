# Quickstart: Linux / macOS (sd4linux)

This guide explains how to use **sd4linux**, a SUPLA-compatible device
implementation running on **Linux and macOS**.

This path is intended for:
- integration with other systems,
- development, testing and debugging,
- running SUPLA-compatible devices on small Linux-based platforms.

It is **not** intended for deeply embedded microcontroller firmware.

---

## What is sd4linux

`sd4linux` behaves like a SUPLA device, but runs as a regular application
on a Linux-based operating system.

It uses the same core **supla-device** library code as Arduino and ESP-IDF
targets, but with a POSIX/Linux runtime.

Typical use cases:
- integrate SUPLA with other software via files, CLI or MQTT,
- simulate a device without flashing firmware,
- test channel logic and state handling,
- run SUPLA-compatible device logic on small Linux platforms.

---

## Supported Linux platforms

In addition to desktop Linux and macOS, sd4linux works well on
**small Linux-based devices**, including but not limited to:

- Raspberry Pi (all common models),
- Raspberry Pi Zero / Zero 2,
- Orange Pi,
- Banana Pi,
- BeagleBone Black,
- similar ARM-based single-board computers.

These platforms are useful when you need:
- more resources than a microcontroller,
- access to Linux services and networking,
- easy integration with other software components.

---

## Repository location

Linux-related code and examples are located under:

- `extras/`

This includes:
- sd4linux implementations (`extras/examples/linux/`),
- platform-specific helpers (`extras/porting/linux/`),
- integration and reference examples.

The core library code remains shared and lives in `src/`.

---

## Prerequisites

Before starting, make sure you have:
- a Linux-based operating system (desktop or SBC),
- a standard C/C++ build environment,
- network access to SUPLA Cloud or a self-hosted SUPLA server.

Exact dependencies depend on the selected example and are documented
in the corresponding `extras/examples/linux/` directories.

---

## Getting started

1. Clone the `supla-device` repository.
2. Browse the `extras/examples/linux/` directory.
3. Follow the README or comments provided with the selected example.
4. Build and run the application on your Linux system.
5. Observe logs and device behavior in the SUPLA user interface.

---

## Runtime model

From a logical perspective, sd4linux follows the same model as embedded devices:
- device logic is built from **Elements**,
- user-visible functionality is exposed as **Channels**,
- the device initializes using `SuplaDevice.begin(...)`,
- the main loop repeatedly calls `SuplaDevice.iterate()`.

The difference lies in the surrounding runtime and operating system services.
It also provides YAML based configuration and file-based state storage.

---

## Limitations

- sd4linux is not optimized for real-time constraints.
- Timing behavior differs from microcontroller-based devices.
- It should not be treated as a reference for low-level hardware control.

---

## Next steps

- Learn how channels are exposed to SUPLA:
  Concepts: Channels (../concepts/channels.md)

- Learn how the device lifecycle works:
  Concepts: Architecture (../concepts/architecture.md)

