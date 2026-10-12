# SupLAN VirtualRelay -> remote Relay via executeAction()

A real local `VirtualRelay`, controlled from Cloud or by a short click on GPIO0,
issues native `ACTION_TURN_ON` / `ACTION_TURN_OFF` for its ON/OFF transitions.
A separate button from GPIO4 to GND issues `ACTION_TOGGLE` directly; the Source
calculates TOGGLE from its current state. It does not toggle the VirtualRelay.

Approve **CONTROL** for a Source power/light relay with Native/9 support.
`Supla::TURN_ON/OFF/TOGGLE` are local SDK actions; the adapter explicitly maps
them to network `ACTION_*` IDs. `executeAction()` encodes protected Native/9;
no application-side framing or keys are required.

CONTROL includes shared READ interest. `canControl()` tests effective rights;
true from `executeAction()` means local admission, not delivery or completion.
The VirtualRelay is a command selector, not confirmed Source state. Its initial
OFF does not send a command. Rejections/timeouts do not replay the old selection
when access returns. Old Source firmware lacking Native/9 can time out even
after local admission. No optimistic remote cache update is performed.

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
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp8266:esp8266:generic --library . --build-path "$example_build/esp8266" examples/esp/SupLanRemoteRelayExecuteAction
TMPDIR=/tmp/codex arduino-cli compile --fqbn esp32:esp32:esp32wrover:PartitionScheme=huge_app --library . --build-path "$example_build/esp32" examples/esp/SupLanRemoteRelayExecuteAction
```
