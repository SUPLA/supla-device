# Quickstart: ESP-IDF (ESP32)

**supla-device** can be used with **ESP-IDF** on ESP32.

This environment provides full control over networking,
memory management and system updates.

---

## Supported targets

- all ESP32 variants with Wi-Fi, which are supported by ESP-IDF

---

## Repository layout (important)

This repository uses an **Arduino library directory layout**.
This is required for Arduino compatibility and does **not** mean Arduino is the
primary or preferred platform.

Practical consequences:
- shared and core code lives in `src/` and must compile as an Arduino library,
- ESP-IDF specific code, examples and tooling are located in `extras/`.

---

## Prerequisites

Before you start, make sure you have:
- ESP-IDF installed and working on your system,
- USB drivers for your ESP32 board,
- a supported ESP32 board connected via USB.

We generally prefer using the newest ESP-IDF releases.
However, right after a new ESP-IDF release, the library or examples may temporarily
fail to compile.

If you encounter build issues:
- try the previous stable ESP-IDF version,
- report the problem and include ESP-IDF version and build logs.

---

## Getting started

1. Clone the supla-device repository.
2. Navigate to the repository directory.
3. Select an example from the `extras/examples/esp-idf` directory.
4. Follow the example-specific README or comments to configure the project.
5. Build, flash and monitor the device using standard ESP-IDF tools.

---

## Runtime model

Devices built with ESP-IDF follow the same core model as Arduino-based devices:
- create network interfaces and device Elements,
- define Channels that represent user-visible functionality,
- call `SuplaDevice.begin(...)` during initialization,
- call `SuplaDevice.iterate()` repeatedly in the main loop or task.

The main difference is the surrounding runtime and project structure.

---

## Next steps

- Learn how the device works internally:
  Concepts: Architecture (../concepts/architecture.md)

- Configure networking:
  How-to: Networking (../howto/networking.md)

- Add persistent storage:
  How-to: Storage (../howto/storage.md)

