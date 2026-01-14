# supla-device documentation

This documentation covers the **supla-device** SDK/library used to build custom SUPLA-compatible devices.

Use it if you are:
- building your own firmware for **ESP32 / ESP-IDF** or **Arduino** (Arduino IDE / PlatformIO),
- prototyping DIY devices (switches, shutters, sensors, HVAC/thermostats),
- integrating SUPLA device behavior into other systems (e.g. via **sd4linux**).

If you are looking for a ready-to-flash firmware, this repository is likely not what you need.

---

## Choose your path

### Arduino library (Arduino IDE / PlatformIO)
Best if you want the quickest start and a large ecosystem of examples/libraries.

- Start here: [Quickstart: Arduino](quickstart/arduino.md)
- Typical targets: ESP8266, ESP32, Arduino Mega (limited support)

### ESP-IDF (ESP32)
Best for advanced projects and commercial/long-term products where you want full control over memory, networking and updates.

- Start here: [Quickstart: ESP-IDF](quickstart/esp-idf.md)

### Linux / macOS (sd4linux)
A ready-to-run SUPLA device implementation for desktop environments, useful for integration, testing and debugging.

- Start here: [Quickstart: sd4linux](quickstart/sd4linux.md)

---

## Core concepts

If you are new to the project, these are the most important concepts:

- **Channels**: what the user sees and controls in SUPLA  
  → [Concepts: Channels](concepts/channels.md)

- **Architecture**: how device code connects to SUPLA server/cloud and how the main loop works  
  → [Concepts: Architecture](concepts/architecture.md)

---

## How-to guides

Practical guides for common tasks:

- Networking setup and interfaces  
  → [How-to: Networking](howto/networking.md)

- Persistent storage (EEPROM/Flash/FRAM) and when it is required  
  → [How-to: Storage](howto/storage.md)

---

## Troubleshooting and reference

- Common problems and fixes  
  → [Troubleshooting](troubleshooting.md)

- Frequently asked questions  
  → [FAQ](faq.md)

- Terms and abbreviations used across the docs  
  → [Glossary](glossary.md)

---

## Contributing

Contributions are welcome. Please note:
- a **CLA** is required (requested automatically after opening a PR),
- repository layout follows Arduino library rules (which affects where code can live).

See: `CONTRIBUTING.md` in the repository root.

