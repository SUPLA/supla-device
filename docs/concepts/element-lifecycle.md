# Element lifecycle

This document describes the **lifecycle of an Element** in **supla-device**
and the order in which Element callbacks are executed.

Understanding the Element lifecycle is essential when:
- implementing custom Elements,
- debugging device behavior,
- designing gateway or dynamic devices.

The lifecycle is identical across platforms
(Arduino, ESP-IDF and Linux).

---

## Overview

An **Element** represents an internal building block of device logic.

During device operation:
- Elements are created by the developer,
- registered internally by supla-device,
- executed in a defined and repeatable order.

The lifecycle consists of:
- initialization callbacks,
- runtime callbacks,
- periodic timer callbacks.

---

## Lifecycle phases

### 1) Creation phase

Elements are created by the developer in firmware.

Important rules:
- all Elements should be created **before** the device runtime starts,
- Elements are automatically registered internally during creation,
- no callbacks are executed at this stage.

At this point:
- configuration is not yet loaded,
- storage is not yet available,
- network is not initialized.

---

### 2) Configuration loading phase

This phase occurs during device startup.

#### onLoadConfig

Called to load **configuration data** for the Element.

Typical use:
- read configuration values from configuration storage,
- restore settings configured via SUPLA Cloud,
- initialize internal parameters based on configuration.

Notes:
- configuration storage may be unavailable if not configured,
- this method should not access hardware.
- Elements that do not require configuration may ignore this callback.

---

### 3) State loading phase

#### onLoadState

Called to load **runtime state** from persistent storage.

Typical use:
- restore counters or positions,
- load calibration data,
- restore last known state.

Notes:
- state storage is optional,
- Elements that do not require state may ignore this callback.

---

### 4) Initialization phase

#### onInit

Called after configuration and state are loaded.

Typical use:
- initialize GPIOs,
- configure peripherals,
- set initial Channel state.

Notes:
- hardware access is expected here,
- configuration and state are already available.

---

## Runtime phase

After initialization, the device enters its main runtime loop.
During this phase, the following callbacks may be executed.

---

### iterateAlways

Called on **every iteration** of `SuplaDevice.iterate()`,
regardless of connection state.

Typical use:
- time-critical logic,
- internal housekeeping tasks,
- state checks independent of network connectivity.

Important:
- this method must be **non-blocking**,
- avoid long operations or delays.

---

### iterateConnected

Called on each iteration **only when the device is connected
and registered** with SUPLA Server.

Typical use:
- sending Channel state updates.

Notes:
- not called when the device is offline,
- should also remain non-blocking.

---

### onRegistered

Called **every time**, after the device successfully registers
with SUPLA Server.

Typical use:
- actions that require confirmed registration,
- initial synchronization with server state.

---

## Timer callbacks

### onTimer

Called periodically (typically every ~10 ms).

Typical use:
- periodic checks,
- debouncing,
- scheduling lightweight tasks.

Important:
- must execute quickly,
- must not block.

---

### onFastTimer

Called at a higher frequency (typically ~1 ms, or ~0.5 ms on Arduino Mega).

Typical use:
- very time-sensitive logic,
- precise timing operations.

Important:
- **extremely time-critical**,
- keep logic minimal,
- avoid any blocking or heavy computation.

---

## State saving phase

### onSaveState

Called periodically during runtime.

Typical use:
- save runtime state to persistent storage,
- store counters or positions.

Notes:
- not called on every iteration,
- storage implementation controls write frequency,
- Elements should assume this method is called infrequently.

---

## Callback execution order (summary)

The typical execution order is:

1. Element creation  
2. onLoadConfig  
3. onLoadState  
4. onInit  
5. onRegistered (once, after successful registration)  
6. Runtime loop:
   - iterateAlways
   - iterateConnected (if connected)
   - onTimer
   - onFastTimer
   - onSaveState (periodically)

---

## Common mistakes

- Creating Elements after the device runtime has started.
- Performing hardware access in onLoadConfig or onLoadState, or earlier.
- Blocking execution inside iterateAlways or iterateConnected.
- Performing heavy logic in onFastTimer.

---

## Best practices

- Keep callbacks short and non-blocking.
- Separate configuration logic from runtime logic.
- Use configuration storage for user-configurable parameters.
- Use state storage only when persistence is required.
- Preserve Channel numbering and Types when modifying firmware.

---

## Summary

- Element lifecycle is deterministic and ordered.
- Initialization and runtime phases are clearly separated.
- Configuration and state are loaded before hardware initialization.
- Runtime callbacks must be non-blocking.
- Timer callbacks are time-critical.
- Understanding lifecycle prevents subtle bugs and instability.

