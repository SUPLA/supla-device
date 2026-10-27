# Server Stream thermometer

Arduino example for ESP8266 and ESP32 using the `suplan` branch of supla-device.
It reads an existing thermometer through `Supla::ServerRemoteChannel` and prints
the latest received temperature every five seconds. No local thermometer Channel
is registered, and Serial at 115200 baud is output-only.

Data arrives over the Device's existing TLS/SRPC connection to SUPLA Server.
The Source can be in another LAN and does not need SupLAN D2D support. There is
no requirement for LAN discovery, a peer session or direct UDP access to the
Source for this delivery mode. A Server/Cloud supporting SERVER_STREAM and READ
approval is required. The example cannot deliver updates while disconnected
from that Server and does not fall back to D2D.

## Setup

1. Replace `THERMOMETER_CHANNEL_ID` with the thermometer's global **ChannelId**
   from SUPLA Cloud, not its local ChannelNumber.
2. Adjust GPIO0 (configuration button connected to GND) and GPIO2 (active-low
   status LED) to your board.
3. Open `ServerStreamThermometer.ino` in Arduino IDE with the `suplan` SDK and
   the dependencies used by its WebInterface example. Select your board and a
   flash layout retaining a LittleFS partition. Check that the application fits
   the selected partition; an arbitrary default ESP32 layout may be too small.
4. Upload the sketch yourself. On first boot connect to its configuration AP
   and open the local WWW interface. Configure Wi-Fi, your SUPLA account and a
   compatible server, for example your `suplan-cloud.supla.org` test account.
   Use TLS on port 2016; SupLAN-enabled registration requires TLS. Save/restart.
   Hold the configuration button to enter configuration mode again.
5. After registration and SERVER identity/sync, the SDK automatically requests
   READ access. Approve the Server Stream request for this Device in Cloud.
   If already authorized, another manual approval may not be needed.

There are no fixed credentials or manual ENSURE/subscription calls in the
sketch. `enableSupLan()` is required to advertise the capability used by Server
Stream, even though this handle receives its data through SRPC. The flag itself
does not grant access to the configured ChannelId.

## Output and availability

The five-second print reads the cached snapshot; it does **not** poll the Server
every five seconds or initiate a new physical temperature measurement. The SDK
maintains the subscription and reconnect behavior.

`accessStatus()` describes authorization/admission, not whether a current value
is available. For example, `AUTHORIZED` alone is insufficient to display a
temperature. The sketch additionally checks the Temperature capability and value
availability, so an unavailable Channel is not displayed as `0 C`.

Offline/expiry is owned by the Server; Server Stream has no local D2D TTL
countdown. Loss of the Destination's SRPC connection makes the stream unavailable.
After reconnect, the SDK restores access/subscription through the existing
registration and identity/sync flow. Approval, authorization and available data
are separate states.

The delivery-mode difference is explicit:

```cpp
// Direct SupLAN D2D:
Supla::RemoteChannel thermometer(ID, Supla::READ);

// READ-only delivery through SUPLA Server:
Supla::ServerRemoteChannel thermometer(ID);
```

## Compile checks

Use unique build/cache directories and at most three compiler jobs:

```sh
example_build=$(mktemp -d /tmp/codex/server-stream-thermometer-XXXXXXXX)
export TMPDIR="$example_build"
export ARDUINO_BUILD_CACHE_PATH="$example_build/cache"
arduino-cli compile --jobs 3 --fqbn esp8266:esp8266:generic \
  --library . --build-path "$example_build/esp8266" \
  examples/esp/ServerStreamThermometer
arduino-cli compile --jobs 3 \
  --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M \
  --library . --build-path "$example_build/esp32" \
  examples/esp/ServerStreamThermometer
```

Run from the SDK repository root. The ESP32 command uses two application slots
with a filesystem (`min_spiffs`); do not substitute a no-OTA layout as evidence
of OTA compatibility. Compilation does not verify physical Wi-Fi, TLS,
registration, Cloud approval, Server delivery or product E2E.

Verified compile/link on 2026-10-27 with Arduino CLI 1.5.1, using the commands
above and the default SDK capacity profiles:

| Target | Arduino core | SDK profile | Result |
| --- | --- | --- | --- |
| ESP8266 generic | 3.1.2 | Constrained | PASS |
| ESP32 classic, 4 MB, `min_spiffs` | 3.3.10 | Standard | PASS |

The ESP32 build uses 1,446,258 bytes of its 1,966,080-byte application slot.
No hardware was flashed or run; physical registration, approval, data delivery,
reconnect and OTA operation remain unverified for this example.
