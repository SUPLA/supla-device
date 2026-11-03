# Reading remote channels

Use `RemoteChannel` or `ServerRemoteChannel` to access an existing SUPLA Channel
by its global **ChannelId** from Cloud. This is not
the device-local ChannelNumber. A handle does not create a local SUPLA Channel
or duplicate the remote value cache. These APIs are not available on AVR.

## Binding pool capacities

`ResourceBindingManager::relayBindingStatus()` reports the stored assignment of a
local PumpSwitch/HOCS relay, independently of peer sessions, values and temporary
authorization availability. `Configured` does not authorize actuation;
`relayDemand()` still applies the live permission and identity-transition checks.
Before storage is loaded, after failed key enumeration or during a full SERVER
scope fence, the status is `Unknown`. With no channel identity mapping, an empty
stored topology proves `None`; a nonempty topology remains `Unknown` rather than
guessing from a reused local channel number. Explicit revoke, empty binding sets
and identity migration remove the corresponding stored assignments. Missing
SERVER identity also means `Unknown` for a matching stored SERVER binding.
Loading binding records starts from an empty runtime set and never writes
storage. Invalid, unreadable or over-capacity records are skipped without retry;
other records still load. Reloading also discards LOCAL sample history, which
returns after a new snapshot.
Explicit migration can remove malformed records, but unreadable records or
records exceeding the compiled per-peer limit keep the migration pending.
Changing an output's function leaves its binding stored and inactive; restoring
PumpSwitch/HOCS can reactivate it under current authority and the existing
value-validity rules. Peer/session loss does not delete bindings.

For HVAC output bindings, `SUPLAN_MAX_RESOURCE_BINDINGS` limits received edges
from remote HVAC Channels to local outputs. `SUPLAN_MAX_BINDING_INTENTS` limits
outgoing pairs from local HVAC Channels to remote outputs. These are separate
pools; neither limit changes the number of remote value resources or consumers.

By default, the intent limit equals `SUPLAN_MAX_RESOURCE_BINDINGS`, preserving
the existing profile capacities. A gateway can explicitly build with, for example,
`SUPLAN_MAX_RESOURCE_BINDINGS=400` and `SUPLAN_MAX_BINDING_INTENTS=32`.
Both limits must be positive. Define overrides consistently for all SDK sources.
`SUPLAN_MAX_BINDINGS_PER_PEER` remains a separate wire/transport bound; changing
the intent limit does not change it. On the ESP32 ABI, 400/32 instead of 400/400
reduces the manager's retained size by 7360 bytes, without heap allocation.

## Choose the delivery mode

| Handle | Delivery | Permissions |
| --- | --- | --- |
| `Supla::RemoteChannel(id, permissions)` | SupLAN D2D: direct LAN communication with the Source | READ, CONTROL, ACTION_TRIGGER |
| `Supla::ServerRemoteChannel(id)` | Server Stream: the existing connection to SUPLA Server | READ only |

D2D needs a SupLAN Source and LAN discovery/UDP connectivity. Once provisioned,
it can operate without Internet or Server connectivity. Server Stream can read
a Source in another LAN, including one without SupLAN D2D support, but depends
on the Destination's Server connection. There is no automatic fallback between
the two modes. Access approval/provisioning is specific to the selected mode.

Choose **one** of these declarations for a thermometer:

```cpp
#include <SuplaDevice.h>
#include <math.h>
#include <supla/channels/remote_channel.h>

Supla::RemoteChannel thermometer(12345, Supla::READ);
```

Or, for delivery through Server:

```cpp
#include <SuplaDevice.h>
#include <math.h>
#include <supla/channels/server_remote_channel.h>

Supla::ServerRemoteChannel thermometer(12345);
```

Keep the handle alive, for example as a global object. Do not construct a new
handle on every loop iteration. Both modes require `enableSupLan()` before
`begin(29)` (or a supported newer protocol), TLS registration, SERVER identity
and synchronization, and the appropriate access grant. The SDK handles ENSURE,
READ refresh, subscriptions and recovery; applications do not implement them.

`configure(newId, permissions)` retargets an existing handle and returns whether
the configuration was accepted. `deactivate()` stops using the handle. Obtain
fresh state after these operations. Deactivation/deletion removes the local
consumer; it does not revoke an already approved Cloud grant.

## Read the latest state

With network/storage configuration provided by your application, the common
read pattern for either declaration above is:

```cpp
bool deviceStarted = false;

void setup() {
  deviceStarted = SuplaDevice.enableSupLan() && SuplaDevice.begin(29);
}

void loop() {
  if (!deviceStarted) return;
  SuplaDevice.iterate();

  const auto state = thermometer.state();
  const double temperature = state.getValueDouble();
  if (state.availableFor(Supla::ChannelCapability::Temperature) &&
      isfinite(temperature)) {
    // Use temperature, in degrees Celsius.
  } else {
    // No usable temperature. Show "unavailable" or apply your safe fallback.
    // Do not substitute 0 C for missing data.
  }
}
```

This reads the latest received value. Calling `state()` does not take a new
physical measurement. Printing every five seconds does not mean polling the
Source or Server every five seconds; the SDK manages communication separately.
`channelState()` is an alias for `state()`.

### Available values and their types

| `ChannelState` method | Meaning |
| --- | --- |
| `available()` | The view currently exposes usable, online data. This alone does not validate a particular numeric value or its type. |
| `availableFor(capability)` | Additionally checks whether the Channel type/function supports the requested role, e.g. `Temperature`. |
| `getValueDouble()` / `getValueDoubleFirst()` | First numeric value; returns `NaN` for unavailable, invalid or unsupported data. Check `isfinite()`. |
| `getValueDoubleSecond()` | Second component where defined; e.g. humidity for a temperature/humidity Channel. Check `isfinite()`. |
| `getValueBool(&value)` | Reads a binary sensor or relay into a `bool`; returns whether the read succeeded. |
| `getValueInt64(&value)` | Reads an impulse counter into a `uint64_t`; returns whether the read succeeded. |
| `binary(&value)` / `hvac(&value)` | Role-checked reads of a binary sensor or `THVACValue`; returns whether the read succeeded. |
| `value(bytes)` | Copies the raw eight-byte value when available; decoding depends on the Channel type. |
| `type()` / `function()` | Channel metadata, not a guarantee of available data. |
| `receivedMs()` / `validityTimeSec()` | Remote snapshot reception time on the monotonic SDK clock and the advertised validity. |

For a binary sensor or relay, distinguish a successful read of **false/OFF**
from a failed read:

```cpp
Supla::RemoteChannel binaryInput(23456, Supla::READ);

void readBinaryInput() {
  const auto state = binaryInput.state();
  bool active = false;
  if (state.getValueBool(&active)) {
    // active may legitimately be false. The read itself succeeded.
  } else {
    // Unavailable data or an incompatible Channel type.
  }
}
```

The return value of `getValueBool()` is read success, not the sensor state.
The same rule applies to the other getters with output pointers.

### Lifetime and freshness

`ChannelState` is a **borrowed, read-only view**, not an owned snapshot. Use it
immediately and obtain a new view after the next `SuplaDevice.iterate()`. Do not
retain it across handle reconfiguration/deactivation, deletion or manager
teardown. Copying the `ChannelState` object does not copy the cached value.
Copy a decoded scalar into your own variable if needed, and separately track
whether you may still use it.

For an available remote snapshot, its reception age can be calculated as
`uint32_t ageMs = millis() - state.receivedMs();`. This is delivery age, not the
age of the Source's physical measurement. A zero timestamp by itself does not
prove that no snapshot exists; use availability checks.

D2D snapshots expire according to the Source's SupLAN validity (default 180 s).
The Source can override this, including zero to disable timed expiration.
A temporary session loss can leave an unexpired cached value available; expiry,
an unavailable Source snapshot or revoked access makes it unavailable. Server
Stream follows Server-owned Offline/expiry and becomes unavailable on loss of
the Destination's Server connection; do not add a local D2D TTL countdown to it.

## Permission is separate from data availability

`hasReadPermission()`, `hasControlPermission()` and
`hasActionTriggerPermission()` describe effective permissions for this handle.
They do not promise transport/session readiness or an available value. Request
READ to read snapshots; CONTROL also implies READ. ACTION_TRIGGER subscribes to
incoming Action Trigger events and is not a permission to execute a CONTROL.

Use `accessStatus()` for diagnostics and `channelAccessStatusName(status)` to
print a readable name:

| Status | Meaning |
| --- | --- |
| `AUTHORIZED` | All permissions requested by this handle are granted. A session or first value may still be missing. |
| `PENDING_APPROVAL` / `REJECTED` | Access is awaiting approval or has been rejected. |
| `PROVISIONING` | The requested permissions are not yet effective. |
| `IDENTITY_UNRESOLVED` | The SERVER identity needed to resolve the handle is not ready. |
| `RESOURCE_LIMIT` | The handle could not be admitted to the bounded manager resources. |
| `INVALID` | Invalid/deactivated handle or unsupported request. |
| `LOCAL_CHANNEL` | A generic `ChannelHandle` resolved to a local Channel; `RemoteChannel` requires a remote target. |

Always use the `state()` checks above to decide whether a value can be read.

For D2D CONTROL, `setValue(value[8], durationMs)` and
`executeAction(actionId, params, length)` require CONTROL permission. A `true`
result means local admission, not confirmation of remote execution or delivery.
Commands are best effort, with bounded transport handling; they do not provide
persistent state synchronization or replay the last selection after reconnect.
Server Stream is READ-only.

## Complete examples

- [SupLAN remote thermometer](../../examples/esp/SupLanRemoteThermometer/README.md)
- [Server Stream thermometer](../../examples/esp/ServerStreamThermometer/README.md)
- [D2D relay via setValue()](../../examples/esp/SupLanRemoteRelaySetValue/README.md)
- [D2D relay via executeAction()](../../examples/esp/SupLanRemoteRelayExecuteAction/README.md)

### Configuration persistence

The SDK accepts a configuration write when its Config setters (or key removal)
and `commit()` succeed; it does not read data back to verify the write or restore
previous staged values after failure. A failed candidate may therefore reach
storage during a later unrelated commit and appear after restart; binding and
association senders can retry their idempotent requests normally. SERVER identity,
root and migration-fence records retain their staging rollback protection.

### Binding storage allocation

Ordinary RemoteChannel READ/CONTROL does not allocate the HVAC binding manager
at startup. Its complete, fixed-capacity storage is allocated on first binding
use, explicit `resourceBindings()` access, or discovery of retained binding
records in `sl-peers`. Existing records are considered without an Internet connection and
before an association replacement can acknowledge removal of related topology.
A Source also uses the manager to send authoritative empty binding sets, so a
Source association can activate it even when the current desired set is empty.
The manager remains allocated until its protocol owner is destroyed; all binding
and consumer capacities are unchanged. If allocation fails, binding operations
fail without acknowledging installation or revocation, and can be retried.

### Peer and binding persistence

SERVER associations and received binding sets share the versioned, CRC-protected
`sl-peers` blob. An association update and its mandatory binding removal are one
Config write and commit; runtime changes follow only after success. The SDK
trusts the Config result without readback. A rejected candidate can still become
visible after restart on backends that wrote it before reporting a commit error.

Unreleased `sl-peer-*` and `slb_*` records are discarded before SERVER identity
is read; they are never converted. A failed cleanup or store read leaves peer
state unloaded and retries at most once per second. Invalid blob structure,
version, checksum or capacity is removed through Config and starts empty;
read/allocation errors leave the stored blob untouched. Full SERVER scope
migration preserves LOCAL binding records. Bindings still require live authority
to act, and their configured status is independent of transport availability.

The whole-store update uses one temporary buffer for the existing blob plus
the incoming record, capped by the profile maximum. It does not reserve the
profile maximum for small configurations. NVS also needs room for the old and new blob during
replacement; the Standard maximum is not guaranteed to fit alongside other
settings in a 16 KiB NVS partition. Board partition sizing remains a product
choice, and failed writes are reported rather than increasing storage limits.
