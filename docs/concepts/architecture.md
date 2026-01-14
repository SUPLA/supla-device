# Architecture

This document explains the high-level architecture of **supla-device**
and how devices interact with the SUPLA platform.

It focuses on concepts common to all supported environments:
Arduino, ESP-IDF and Linux (sd4linux).

---

## High-level overview

A SUPLA-based system consists of the following main components:

- **Device** – built using supla-device
- **SUPLA Server (core)** – central backend component
- **SUPLA Cloud** – frontend and configuration layer
- **Client applications** – mobile apps and other user interfaces
- **Database** – persistent storage for configuration and state

The general data flow is:

Device  
→ SUPLA Server (core)  
↔ Database  
↔ SUPLA Cloud  
↔ Client applications

Devices do **not** connect directly to SUPLA Cloud.

---

## Device architecture

A SUPLA device is built from a small number of core components:

- **Elements** – internal building blocks implementing device logic
- **Channels** – user-visible device functions
- **Network interface** – connectivity to SUPLA Server
- **Configuration storage (optional)** – persistent device configuration
- **State storage (optional)** – persistent runtime state

All device logic is hosted inside a single logical **SuplaDevice** instance.

---

## Elements and Channels

### Elements

**Elements** implement internal device behavior.

They:
- manage hardware or virtual state,
- implement logic and control flow,
- may exist without exposing anything to the user.

Elements are created by the developer and registered automatically
when the device starts.

### Channels

**Channels** represent functionality visible to users and applications.

They:
- expose device state and control to SUPLA,
- are linked to Elements,
- are configured and presented via SUPLA Cloud and client apps.

You can think of:
- Channels as *what the user sees*,
- Elements as *how the device works internally*.

---

## Device lifecycle

### Initialization phase

During startup:
- network interface is initialized,
- Elements are created,
- configuration is loaded from **Configuration storage (if used)**,
- runtime state is loaded from **State storage** (if used),
- Elements are initialized,
- timers and background tasks are started.

In general, all Elements should be created **before** the device runtime is started.

### Runtime phase

After successful initialization, the device enters its main runtime loop.

During runtime:
- the device connects and maintains connection to the network (Wi-Fi, Ethernet),
- the device connects and maintains connection to SUPLA Server,
- incoming commands are processed,
- channel state updates are sent,
- Element lifecycle callbacks are executed,
- timers and background tasks are handled.

The runtime loop is driven by repeated calls to `SuplaDevice.iterate()`.

For a detailed description of the Element callback order and lifecycle,
see: [Concepts: Element lifecycle](element-lifecycle.md).

---

## Channel order and registration

Channels are assigned identifiers based on the **order of creation**
during device startup.
Channel number can be changed manually.

Important consequences:
- changing the order of Channel creation changes their identifiers,
- removing or adding Channels changes the device structure,
- SUPLA Server will reject registration if the structure changes (it accepts
    channel additions).

If the Channel structure changes:
- check in Cloud if "channel conflict" is visible. In some cases deleting conflicted channel will remove the conflict.
- remove the device from SUPLA Cloud (as a last resort),
- register it again from scratch.

This is a common source of registration issues.

---

## Configuration storage

**Configuration storage** is used to store device configuration that must
persist across restarts and power cycles.

It typically contains:
- network configuration (SSID, password, Ethernet settings),
- SUPLA connection details (server address),
- device credentials (email, GUID, AUTHKEY),
- channel and Element configuration set via SUPLA Cloud or local configuration.

---

## Configuration vs state storage

It is important to distinguish between two types of persistent storage.

### Configuration storage
- stores **configuration data**,
- written infrequently,
- loaded during device initialization,
- modified via SUPLA Cloud or local configuration mechanisms.

Examples:
- Wi-Fi credentials,
- server address,
- channel function assignments,
- Element parameters.

### State storage
- stores **runtime state**,
- updated periodically during operation,
- used to restore device state after restart,
- in some cases part of the configuration for Elements is also stored (legacy).

Examples:
- impulse counter values,
- roller shutter positions,
- calibration data.

Some Elements require state and/or configuration storage to function correctly.

---

## SUPLA Server (core)

SUPLA Server is the **central backend component** of the platform.

It is responsible for:
- handling all device connections,
- maintaining current device and channel state,
- processing commands from client applications,
- communicating with the database,
- providing real-time data flow.

All real-time communication passes through SUPLA Server.

---

### Network ports and encryption

SUPLA devices connect to SUPLA Server using the following TCP ports:

- **2016** – encrypted connection (TLS)  
  This is the default and recommended option.

- **2015** – unencrypted connection  
  This mode is **deprecated** and should not be used for new devices.
  It is kept only for backward compatibility.

Devices should always use **TLS on port 2016** unless there is a very specific
reason not to do so (e.g. legacy testing environments, Arduino Mega).

Both ports may still be available on the server side, but unencrypted
connections should be considered insecure.

Unencrypted connections may be removed in the future.

---

## SUPLA Cloud

SUPLA Cloud acts primarily as a **frontend and configuration layer**.

It is responsible for:
- device and channel configuration,
- user and account management,
- presentation of device state and history,
- exposing APIs for client applications.

SUPLA Cloud:
- communicates with SUPLA Server,
- accesses the database directly,
- does not accept direct device connections.

---

## Client applications

Client applications include:
- mobile apps,
- web interfaces,
- external integrations.

They:
- communicate with SUPLA Server (directly or via Cloud APIs),
- present device state to users,
- allow control of Channels based on permissions.

Visibility of devices and Channels depends on:
- Locations,
- Access Identifiers assigned to clients.

---

## Summary

- supla-device provides a common device-side architecture.
- Devices connect only to SUPLA Server, not directly to Cloud.
- Elements implement logic, Channels expose functionality.
- Configuration and state are stored separately.
- SUPLA Server is the system core.
- SUPLA Cloud handles configuration and presentation.
- Client visibility depends on configuration and permissions.

Understanding this architecture helps avoid common pitfalls
and makes troubleshooting significantly easier.

