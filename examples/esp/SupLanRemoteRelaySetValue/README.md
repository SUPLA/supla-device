# SupLAN VirtualRelay -> remote Relay via setValue()

A real local `VirtualRelay` is a command input visible in Cloud. A short click
on GPIO0 also toggles it. Its ON/OFF transitions call `setValue(value[8])` on
a CONTROL `ChannelHandle`; byte 0 carries ON/OFF and the other bytes are zero.
The virtual channel has its own normal local identity; it does not represent
the remote resource or duplicate its cache.

Approve **CONTROL** for a Source power/light relay. CONTROL also holds a shared
READ interest automatically. `hasControlPermission()` checks effective
authorization,
not reachability. `setValue()` returning true means bounded local admission,
not receipt, physical completion or an updated remote value.

The VirtualRelay indicates the local command selection, not confirmed remote
state. Initialization/first identity binding does not synchronize its OFF state
to the Source. A rejected or timed-out command is logged without retrying an old
selection when access returns. Make another ON/OFF transition to issue a new
command. Ordinary Cloud control of the Source remains independent.

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
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp8266:esp8266:generic --library . --build-path "$example_build/esp8266" examples/esp/SupLanRemoteRelaySetValue
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp32:esp32:esp32wrover:PartitionScheme=huge_app --library . --build-path "$example_build/esp32" examples/esp/SupLanRemoteRelaySetValue
```
