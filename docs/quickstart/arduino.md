# Quickstart: Arduino library (Arduino IDE / PlatformIO)

Use **supla-device** as an **Arduino library** with **Arduino IDE**
or **PlatformIO**.

This is the most common entry point for DIY projects, prototyping
and early validation of SUPLA-compatible devices.

---

## Supported boards

- ESP8266
- ESP32
- Arduino Mega (limited support due to RAM constraints)

Arduino Uno is **not supported**.

---

## Installation

### Arduino IDE

#### 1) Install Arduino IDE
Install the latest Arduino IDE from the official Arduino website.

#### 2) Install board support packages

**ESP8266**
1. Open **File → Preferences**
2. Add the following URL to **Additional Boards Manager URLs**:
   - `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Open **Tools → Board → Boards Manager**
4. Install **ESP8266 by ESP8266 Community**

**ESP32**
1. Open **File → Preferences**
2. Add the following URL to **Additional Boards Manager URLs**:
   - `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Open **Tools → Board → Boards Manager**
4. Install **esp32 by Espressif Systems**

> We generally prefer the newest board packages, but immediately after a new
> release the library may temporarily fail to compile.
> If you encounter build issues, try the previous stable version and report it.

#### 3) Install supla-device library
1. Open **Tools → Manage Libraries...**
2. Search for **SuplaDevice**
3. Install the latest version

---

### PlatformIO

#### 1) Create a new project
Create a new PlatformIO project and select your board (ESP8266 or ESP32).

#### 2) Add library dependency
Add the following to `platformio.ini`:

  lib_deps = SuplaDevice


PlatformIO will download the library automatically.

> PlatformIO uses its own platform versions.
> If you experience build issues, consider pinning the platform version
> or updating PlatformIO Core.

---

## Run an example

### Arduino IDE
- Open **File → Examples → SuplaDevice**
- Select an example matching your board
- Follow the comments in the example to configure credentials and options
- For ESP boards, the best start will be WebInterface example

### PlatformIO
- Copy one of the Arduino examples into your project
- Adjust `platformio.ini` if needed
- Build and upload using PlatformIO tools

---

## Minimal device flow

An Arduino-based SUPLA device typically:
- creates a network interface,
- creates one or more Elements / Channels,
- calls `SuplaDevice.begin(...)`,
- repeatedly calls `SuplaDevice.iterate()`.

---

## Next steps

- Networking setup: [How-to: Networking](../howto/networking.md)
- Persistent storage: [How-to: Storage](../howto/storage.md)
- Core concepts: [Channels](../concepts/channels.md)

