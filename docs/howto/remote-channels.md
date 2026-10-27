# Reading remote channels

Use `RemoteChannel` or `ServerRemoteChannel` to access an existing SUPLA Channel
by its global **ChannelId** from Cloud. This is not
the device-local ChannelNumber. A handle does not create a local SUPLA Channel
or duplicate the remote value cache. These APIs are not available on AVR.

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
