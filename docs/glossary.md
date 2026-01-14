# Glossary

---

## SUPLA
An open-source smart home platform consisting of devices, cloud/server,
APIs and user applications.

---

## supla-device
A device-side library / SDK used to build custom SUPLA-compatible devices.
It provides networking, protocol handling and a framework for device logic.

---

## DIY
Do-it-yourself projects built by hobbyists or enthusiasts using
supla-device as a foundation.

---

## Channel
A **Channel** represents a single device function that is visible to the
SUPLA server and user interfaces.

Examples:
- relay,
- temperature sensor,
- roller shutter,
- thermostat.

Channels define **what the user sees and controls**.

---

## Element
An **Element** is an internal building block of device software.

Elements:
- implement device logic,
- manage hardware or virtual state,
- may contain one or more Channels,
- may exist without exposing any Channel.

Elements define **how the device works internally**.

---

## Device
A SUPLA **device** is a single logical unit registered in SUPLA server or
a self-hosted SUPLA server.

A device consists of:
- one or more Elements,
- zero or more Channels,
- a unique identity and configuration.

---

## SUPLA Cloud
The **SUPLA Cloud** is primarily a **user-facing front end** for the SUPLA platform.

It is responsible for:
- device and channel configuration,
- presenting device state and history,
- user and account management,
- exposing APIs for client applications.

SUPLA Cloud communicates with:
- SUPLA Server (core),
- the database directly.

Devices do **not** connect directly to SUPLA Cloud.
---

## SUPLA Server (core)
The **SUPLA Server (core)** is the central backend component of the SUPLA platform.

It is responsible for:
- handling all device connections,
- maintaining device state,
- processing channel data,
- communicating with client applications (mobile apps, integrations),
- persisting data in the database.

SUPLA Server is the **heart of the system**:
- devices connect to the server,
- user applications connect to the server,
- SUPLA Cloud communicates with the server and the database.

All real-time device communication flows through SUPLA Server.

SUPLA platform offers free servers for all users. It is also possible to
self-host SUPLA Server.

---

## GUID
A **Global Unique Identifier** used to uniquely identify a device in SUPLA.

The GUID is generated once and must remain unchanged for the lifetime
of the device.

---

## AUTHKEY
An **authentication key** used together with the device GUID to authorize
a device when connecting to SUPLA.

The AUTHKEY is generated once and must remain unchanged for the lifetime
of the device.

---

## Network interface
An abstraction responsible for connecting the device to the network
and SUPLA server.

Examples:
- Wi-Fi (ESP8266, ESP32),
- Ethernet (Arduino Mega, ESP32).

---

## Storage
A persistent memory abstraction used to store device state across restarts.

Storage is required by some Elements and Channels (e.g. impulse counters,
roller shutters).

Examples:
- EEPROM,
- Flash memory,
- FRAM.

---

## iterate()
A method that must be called repeatedly during device operation.

`SuplaDevice.iterate()`:
- handles communication with SUPLA server (including registration),
- executes Element lifecycle methods,
- processes state updates.

---

## begin()
Initialization method that starts the SUPLA device runtime.

`SuplaDevice.begin(...)`:
- initializes networking,
- loads configuration and state,
- initializes Elements.

In general, all Elements should be created **before** calling `begin()`.

---

## Arduino library layout
A repository structure required by the Arduino build system.

Consequences:
- all code in `src/` must compile as an Arduino library,
- some implementations may be placed in `.h` files instead of `.cpp`,
- the layout affects both Arduino and ESP-IDF users.

---

## ESP-IDF
Espressif’s official development framework for ESP32-based devices.

It provides:
- FreeRTOS,
- networking stacks,
- build system and tooling.

---

## sd4linux
A SUPLA device implementation running on Linux or macOS.

Used for:
- integration,
- testing and debugging,
- running SUPLA devices on small Linux platforms.

---

## extras/
A directory in the repository containing:
- platform-specific code,
- ESP-IDF and Linux examples,
- tooling and reference implementations.

Code in `extras/` must not break Arduino library builds.

---

## Commercial device
A product intended for sale or deployment in production environments.

Such devices typically use ESP-IDF and may require additional work
for compliance, updates and long-term maintenance.

