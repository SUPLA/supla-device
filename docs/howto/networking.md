# Networking

**supla-device** manages networking end-to-end:
from network configuration to connection handling
and communication with SUPLA Server.

Network configuration and stability directly affect
device reliability and runtime behavior.

---

## Overview

SUPLA devices communicate with the platform using a **persistent TCP connection**
to **SUPLA Server (core)**.

Key characteristics:
- devices initiate the connection,
- communication is stateful and long-lived,
- all real-time data flows through SUPLA Server,
- SUPLA Cloud is not involved in direct device communication.

---

## Network management model

In **supla-device**, networking is managed by the framework itself.

This includes:
- storing and loading network configuration,
- initializing network interfaces,
- establishing the connection to SUPLA Server,
- maintaining the connection and handling reconnects.

From the application perspective, supla-device acts as an **SDK** rather than
a simple library. Device firmware is expected to integrate with the networking
model provided by supla-device, not replace it.

---

## Important note for Arduino-based environments

In the Arduino ecosystem it is common to manually create and manage
network interfaces (Wi-Fi or Ethernet) directly in user code.

This approach does **not** align with the supla-device architecture.

Treat supla-device as the **owner of the network layer**,
and focus device code on Elements, Channels and application logic.

Other approaches are possible, but are more difficult and not recommended.

---

## SUPLA Server connection

Devices connect to SUPLA Server using the following TCP ports:

- **2016** – encrypted connection (TLS)  
  This is the default and recommended option.

- **2015** – unencrypted connection  
  This mode is **deprecated** and should not be used for new devices.
  It is kept only for backward compatibility.

Unencrypted connections should be considered insecure
and may be removed in the future.

---

## Network interfaces

supla-device provides a **network interface abstraction**.

A network interface is responsible for:
- establishing network connectivity,
- maintaining the connection,
- providing a transport layer for SUPLA protocol.

The exact implementation depends on the platform.

---

## Supported network types

### Wi-Fi

Wi-Fi is the most common network type for SUPLA devices.

Used on:
- ESP8266,
- ESP32.

---

### Ethernet

Ethernet is supported on selected platforms.

Used on:
- Arduino Mega (via Ethernet shields),
- ESP32 (native Ethernet or external PHY).

---

## Platform notes

Networking behavior is largely the same across all supported platforms.

### Arduino library (Arduino IDE / PlatformIO)
- Used with Arduino IDE and PlatformIO.
- Supports ESP8266, ESP32 and Arduino Mega boards.
- On ESP32, it uses the same underlying networking stack as ESP-IDF.

### ESP-IDF (ESP32)
- Native ESP32 environment.
- Networking class is independent from Arduino library, but it should provide the same behavior.

### Linux / sd4linux
- Uses the host operating system networking stack.
- Network configuration is handled outside of supla-device.

### Network implementation layout

- Core network interfaces and abstractions are implemented in  
  `src/supla/network`.

- Platform-specific implementations are located in:
  - `extras/porting/esp-idf` for ESP-IDF,
  - `extras/porting/linux` for Linux / sd4linux.

---

## Configuration storage and networking

Network configuration is typically stored in **configuration storage**.

This may include:
- Wi-Fi SSID,
- Wi-Fi password,
- server address.

Configuration:
- is loaded during device startup,
- can be modified via local configuration mechanisms,
- persists across device restarts.

---

## Connection lifecycle

During device operation:
- the network interface initializes connectivity,
- the device attempts to connect to the network (Wi-Fi, Ethernet),
- the device attempts to connect to SUPLA Server,
- connection state is monitored continuously,
- reconnection is attempted automatically on failure.

Connection attempts may block execution temporarily,
depending on platform and network implementation.

---

## Common networking issues

### Device cannot connect to SUPLA Server
- incorrect server address,
- blocked TCP ports (2015 / 2016),
- DNS resolution failure,
- incorrect TLS configuration (i.e. server certificate not signed by the Supla CA).

---

## Common Wi-Fi connection issues

Problems with Wi-Fi connectivity are one of the most common causes of
device startup.

---

### Incorrect Wi-Fi credentials

The device fails to connect if SSID or password is incorrect.

Things to check:
- SSID name matches exactly (case-sensitive),
- password is correct and up to date,
- make sure that there is no extra space (white space) added,
- credentials were not changed in the access point.

---

### Unsupported Wi-Fi band or mode

Many embedded Wi-Fi devices support **2.4 GHz only**.

Common problems:
- access point configured for 5 GHz only,
- mixed modes with incompatible settings,
- enterprise or uncommon authentication methods.

Ensure the network uses:
- 2.4 GHz band,
- standard WPA/WPA2-PSK authentication.

---

### Weak or unstable signal

A poor Wi-Fi signal may prevent association or cause repeated reconnects.

Typical causes:
- large distance from the access point,
- walls or metal obstacles,
- heavy RF interference from other networks.

Mitigation:
- reduce distance to the access point,
- change Wi-Fi channel,
- improve antenna placement,
- modernize your home network.

---

### Router configuration restrictions

In practice, many connection issues are caused by router configuration
rather than device firmware.

Common examples:
- MAC address filtering enabled,
- exhausted DHCP address pool,
- disabled DHCP server,
- strict access control rules.

Verify router configuration allows new devices to connect.

---

### Wi-Fi client limit on access point

Many consumer Wi-Fi access points have a **limit on the number of connected clients**
(often around **20-30 devices**).

When this limit is reached:
- new devices may fail to connect,
- existing devices may disconnect,
- connections may be dropped intermittently.

Things to check:
- number of devices currently connected to the access point,
- access point documentation for client limits.

For installations with many devices, consider:
- using multiple access points,
- upgrading to enterprise-grade Wi-Fi equipment,
- distributing devices across different networks.

---

### DHCP or IP address issues

The device may associate with Wi-Fi but fail to obtain a valid IP address.

Possible causes:
- DHCP server disabled,
- IP address pool too small,
- IP conflicts on the network.

In such cases the device cannot reach SUPLA Server
even though Wi-Fi appears connected.

---

### Power supply instability

Wi-Fi transmissions generate short current spikes.
An insufficient or unstable power supply can cause device resets during Wi-Fi enabling.

Ensure:
- adequate current capability,
- proper voltage regulation,
- sufficient decoupling capacitors.

---

### Environmental interference

Dynamic environmental factors can affect Wi-Fi reliability:

- crowded RF environment,
- multiple overlapping networks,
- moving obstacles.

Symptoms often include:
- connection working only close to the access point,
- unstable behavior depending on device location.

---

### Testing with a temporary mobile access point

As a debugging step, it is often helpful to create a **temporary Wi-Fi access point**
using a mobile phone and connect the device to it.

This allows you to:
- isolate device-side issues from router configuration problems,
- verify that Wi-Fi credentials and connection logic are correct,
- test connectivity in a clean and controlled environment.

If the device connects successfully to the mobile access point
but not to the target network, the issue is likely related to
router configuration, signal quality or network restrictions.

---

### Debugging tips

When diagnosing Wi-Fi issues:
- verify SSID and password first,
- check access point logs if available,
- test with a simple, known-good Wi-Fi network,
- temporarily reduce distance to the access point,
- observe device logs during connection attempts.

---

## Best practices

- Prefer **TLS on port 2016**.
- Use stable power supply, especially for Wi-Fi devices.
- Avoid blocking operations in Element callbacks.
- Monitor connection stability during long-term tests.
- For complex or commercial devices, prefer ESP-IDF over Arduino.


