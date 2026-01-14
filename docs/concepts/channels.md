# Channels

This document explains how **Channels** work in **supla-device**,
how they are registered, configured and how they become visible to users.

Channels define **what the user sees and controls** in SUPLA.

---

## What is a Channel

A **Channel** represents a single, user-visible device function.

Examples include:
- relay,
- temperature sensor,
- roller shutter,
- thermostat,
- impulse counter.

Channels are:
- created in device firmware,
- registered in SUPLA Server during device startup,
- configured and presented via SUPLA Cloud and client applications.

---

## Channels vs Elements

Channels and **Elements** serve different roles:

- **Elements** implement internal device logic.
- **Channels** expose selected functionality of Elements to SUPLA.

An Element:
- may contain one or more Channels,
- may exist without exposing any Channel.

You can think of:
- Channels as *what the user sees*,
- Elements as *how the device works internally*.

---

## Channel implementation

Core Channel implementations are located in:

- `src/supla/channels/`

This directory contains:
- base Channel classes,
- concrete Channel Type implementations,
- logic shared across all supported platforms.

---

## Channel creation, order and numbering

Channels are created during device startup.

### Creation order
By default, Channels are assigned numbers based on the **order of creation**
in firmware.
If you remove a Channel and do not assign numbers explicitly, remaining Channels
will be compacted on the next start.

Example:
- originally created Channels: 0, 1, 2, 3, 4
- Channel 1 is removed
- during next startup, remaining Channels are created as: 0, 1, 2, 3

This behavior is often **not desired**, especially in gateway-like devices.

---

### Manual channel numbering (firmware)

Channel numbers can be **assigned manually in firmware**.

This is primarily intended for:
- gateway devices,
- devices where Channels may be added or removed dynamically,
- preserving stable numbering of existing Channels.

Example:
- original Channels: 0, 1, 2, 3, 4
- Channel 1 is removed
- remaining Channels keep their numbers: 0, 2, 3, 4

Without manual numbering, the device would recreate them as:
0, 1, 2, 3

In code this is done via the Channel API (e.g. `setChannelNumber(...)`).

## Channel registration and conflicts

SUPLA Server validates device structure during registration.

Important rules:
- adding new Channels is accepted by the server,
- removing Channels or changing existing structure may cause conflicts,
- reordering Channels without proper numbering may cause conflicts.

### Channel conflict

A **channel conflict** occurs when the server detects a mismatch between:
- Channels registered previously (channel's type has to be preserved),
- Channels reported by the device after a firmware change.

If a conflict occurs:
- SUPLA Cloud will display a channel conflict warning,
- in some cases deleting the conflicted Channel in Cloud is sufficient,
- as a last resort, the device must be removed and registered again.

Channel conflicts are a common source of registration problems
after DIY firmware changes.

---

## Channel type and function

Each Channel has a **Type** and a configurable **Function**.

### Channel Type (registration contract)
Channel Type defines the structural meaning of the Channel from the server perspective
and must remain stable.

- Type is validated during device registration.
- Changing the Type of an existing Channel is treated as a structural change and may
  cause conflicts or rejected registration.

### Channel Function (configuration)
Within a given Type, SUPLA Cloud allows selecting a Function that affects behavior
and UI presentation.

- Function is not validated during registration (Type is).
- Function can be changed in configuration without firmware changes.
- Function set to `"none"` is not shown in client apps.

---

## Channel visibility in client applications

For a Channel to be visible in mobile and other client applications,
all of the following conditions must be met:

- the Channel is registered by the device,
- a valid Function is selected (not "none"),
- the Channel is assigned to a Location,
- **"Show on the Client’s devices"** option is enabled,
- the Location is visible for the Access Identifier used by the client.

If any of these conditions is not met, the Channel:
- may be visible in SUPLA Cloud,
- but will not appear in client applications.

---

## Locations and Access Identifiers

Channels are not shown directly to users.

Visibility is controlled by:
- **Locations** – logical groupings of devices and Channels,
- **Access Identifiers** – permissions assigned to client applications.

A Channel is visible to a client only if:
- it is assigned to a Location,
- the Location is accessible by the client’s Access Identifier.

This enables fine-grained access control and multi-user setups.

---

## Channel state and updates

Channels:
- report state changes to SUPLA Server,
- receive control commands from clients via the server.

State updates:
- set a new Channel value (via the Channel API, e.g. `setNewValue(...)`),
- are triggered by Element logic,
- are sent during `SuplaDevice.iterate()` execution,

Channels may also be marked as online/offline using the Channel API,
which is useful for gateway-like devices and integrations.

---

## Platform considerations

The Channel model is identical across all supported platforms:
- Arduino,
- ESP-IDF,
- Linux (sd4linux).

Platform differences affect:
- timing and scheduling,
- available resources,
- storage implementations.

The conceptual Channel behavior remains the same.

---

## Summary

- Channels define user-visible device functionality.
- They are created in firmware and configured in SUPLA Cloud.
- Channel order and numbering are critical for device registration.
- Channel conflicts can often be resolved in SUPLA Cloud.
- Visibility depends on Function, Location and permissions.
- Channel behavior is platform-independent.

Understanding Channels is essential for building stable
and upgrade-friendly SUPLA devices.
