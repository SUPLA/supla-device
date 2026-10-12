# SupLAN remote temperature -> local Relay with hysteresis

A READ `RemoteChannel` feeds two `Condition` bindings: below 20 C turns the
local relay ON, above 22 C turns it OFF. Inside that band it keeps its state.
Approve **READ** for a Source thermometer. The local active-high relay module
uses GPIO5 and starts OFF; adjust polarity for your module.

The first usable snapshot establishes a baseline without firing an action.
Subsequent valid changes evaluate the thresholds. Invalid/expired/revoked
values never become zero temperature and do not fire the bindings. While
unavailable the relay keeps its last state; no automatic fail-off is configured. Reachability loss alone does not
invalidate a still-valid cached value.

## Setup

1. Set the `REMOTE_*_CHANNEL_ID` constant to the Source's global **ChannelId**
   from Cloud, not its local ChannelNumber. Adjust the GPIO constants.
2. Open the `.ino` in Arduino IDE and select ESP8266 or ESP32. Install the SDK
   dependencies used by `WebInterface`. ESP32 needs a larger application
   partition, e.g. `Huge APP (3MB No OTA/1MB SPIFFS)` on a 4 MB board, with
   filesystem space for LittleFS. Upload this layout through USB.
3. The first boot opens configuration mode. Connect to its AP and configure
   Wi-Fi, SUPLA server and account through WebInterface. GPIO0 connects to GND
   for the configuration button; hold it to return to configuration mode.
   GPIO2 is the active-low status LED. Serial at 115200 is for logs only.
4. Use an M5-capable Server/Cloud and SupLAN-capable Source. Approve the demand
   described below. Approval must be followed by normal M2 peer/Expected/key
   provisioning. Both Devices need LAN multicast discovery and UDP connectivity.

`enableSupLan()` before `begin(29)` creates the SDK-owned stack. `iterate()`
services it. No extra loop, static peers or keys are needed. Access changes are
logged; initialization does not authorize arbitrary ChannelIds. Deleting a
handle or reflashing does not revoke a Cloud grant. Persisted provisioning can
operate without Server/WAN after restart; a newly requested relation still
needs approval/provisioning. These sketches are not product E2E evidence.

## Compile checks

From the SDK repository root:

```sh
example_build=$(mktemp -d /tmp/codex/suplan-example-XXXXXXXX)
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp8266:esp8266:generic --library . --build-path "$example_build/esp8266" examples/esp/SupLanRemoteTemperatureLocalRelay
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp32:esp32:esp32wrover:PartitionScheme=huge_app --library . --build-path "$example_build/esp32" examples/esp/SupLanRemoteTemperatureLocalRelay
```
