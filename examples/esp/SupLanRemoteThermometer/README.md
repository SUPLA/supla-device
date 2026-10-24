# SupLAN remote thermometer

One Arduino sketch for ESP8266 and ESP32. Reads an existing thermometer on
another SupLAN-capable Device and logs its temperature every five seconds.
No local Channel is created to represent the remote thermometer. Serial at
115200 baud is output-only; it does not accept commands.

## Setup

1. Replace `REMOTE_THERMOMETER_CHANNEL_ID` with the thermometer's global
   **ChannelId** from SUPLA Cloud. Do not enter its local ChannelNumber.
2. Adjust the configuration button (GPIO0) and status LED (GPIO2, active-low)
   for your board. The button connects GPIO to GND.
3. Build/upload this folder as an Arduino sketch. Select your ESP8266 or ESP32
   board and install the dependencies used by the SDK WebInterface example.
   For ESP32, select a partition with at least 2 MB application space; the
   tested `Huge APP (3MB No OTA/1MB SPIFFS)` layout works on a 4 MB board.
   The default 1.2 MB application partition is too small. This layout disables
   OTA, so upload through USB; retain a filesystem partition for LittleFS.
4. On first boot, connect to the device's configuration AP and open its web
   interface. Set Wi-Fi, SUPLA server and account parameters, then save/restart.
   Hold the configuration button to enter configuration mode again.
5. Source and Destination must use a Server/Cloud with M5 manual approval
   support. Approve this Device's READ request in Cloud. The normal M2
   provisioning flow installs the peer credential and Expected resources.
6. Both Devices must be on a network permitting SupLAN multicast discovery
   (`239.255.201.6:2016`) and UDP communication.

The handle requests READ automatically after registration and identity readiness.
Approval alone is not sufficient: until provisioning and the first usable
snapshot arrive, the log reports unavailable. Invalid, expired or revoked values
are never printed as zero temperature. Persisted identity/associations use the
same LittleFS configuration backend as the normal SDK; there are no static peer
fixtures or hard-coded cryptographic keys.

`SuplaDevice.enableSupLan()` declares opt-in before `begin(29)`, which creates
the production SERVER stack and platform UDP/crypto internally. A successful
enable call records intent; `begin()` confirms initialization. The SDK drives
it through `SuplaDevice.iterate()`; no extra runtime loop or adapter is needed.
Initialization can happen without an IP/WAN connection; UDP starts/retries
when the SDK network becomes ready. Enabling SupLAN does not authorize arbitrary
ChannelIds. Changing the ChannelId in the sketch and reflashing does not revoke
a previously approved Cloud grant.

This example needs both Arduino-platform compile checks and later normal
registration/approval/LAN acceptance. Compilation does not prove product E2E.

`accessStatus()` describes authorization and admission. `AUTHORIZED` means
that all requested permissions are granted, independently of the peer session
or snapshot availability. Use `state().available()` to decide whether data can
be read; `ChannelState::offline()` has been removed. A missing session can be
established on demand and does not by itself mean the peer is offline.

## Compile checks

From the SDK repository root, using unique build directories under `/tmp/codex`:

```sh
example_build=$(mktemp -d /tmp/codex/suplan-thermometer-XXXXXXXX)
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp8266:esp8266:generic --library . --build-path "$example_build/esp8266" examples/esp/SupLanRemoteThermometer
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp32:esp32:esp32wrover:PartitionScheme=huge_app --library . --build-path "$example_build/esp32" examples/esp/SupLanRemoteThermometer
```

## Cache freshness and troubleshooting

The five-second print is a cached snapshot, not a new sensor measurement.
It includes snapshot age, D2D TTL, a named access status and peer/session
reachability independently of value availability. A valid cached temperature can
remain available briefly after losing the peer; after its lease expires the
example prints `unavailable`. The Source default is 180 seconds. The Destination
continues normal READ/recovery after Source restart, even if its reset hint is
lost. No manual Serial commands or fixed credentials are needed.

Source applications can override a Channel with
`channel->setSupLanValidityTimeSec(30)` (or 600, or explicit zero to disable timed
expiry). This does not change D2S validity or sleep mode. SERVER_STREAM follows
Server Offline/expiry, without a local countdown.

See [SDK validity and diagnostic notes](../../../docs/suplan-validity-diagnostics.md)
for measurement-vs-delivery freshness, log levels and troubleshooting. Host tests
and compilation do not verify Wi-Fi/Ethernet multicast on physical hardware.
