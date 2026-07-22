# MQTT documentation scenario coverage

This inventory was prepared from all tests in `extras/test/Mqtt/`. “Captured”
means that an explicit documentation scenario currently surrounds the real
production call. Traffic outside such scopes is deliberately ignored.

| Area | Existing production calls exercised by tests | Publish topics / payloads | Subscribe or command topics | HA Discovery | Captured |
|---|---|---|---|---|---|
| Device state | `publishDeviceStatus(true)` and conditional Wi-Fi state publication | connected, MAC, IP, uptime, connection uptime, RSSI, Wi-Fi signal strength | — | availability is referenced by discovery | yes (including Wi-Fi diagnostics) |
| Relay / power switch | `publishChannelState`, `subscribeChannel`, `processData` | `state/on`; boolean, or `closed` for impulse relays | `set/on`, `execute_action`; parser variants are covered in `mqtt_process_data_tests.cpp` | switch/light and impulse variants | yes (switch and impulse state; subscriptions) |
| Roller shutter | `publishChannelState`, `subscribeChannel` | `state/is_calibrating`, `state/shut`, and `state/tilt` for facade blinds | closing percentage, tilt, execute action | roller, blind, awning, curtain and related variants | yes (roller and facade-blind state) |
| Thermometer | `publishChannelState` | temperature; decimal | read-only | temperature sensor | yes |
| Humidity + temperature | `publishChannelState` | humidity and temperature; decimal | read-only | two discovery entities | yes |
| Dimmer | `publishChannelState`, `subscribeChannel` | brightness and on; integer/boolean | brightness and execute action | light | yes |
| RGB | `publishChannelState`, `subscribeChannel` | color brightness, color, on | color, color brightness, execute action | light | yes |
| Dimmer + RGB | `publishChannelState`, `subscribeChannel` | dimmer/RGB on, brightness and color | separate execute actions, brightness and color | two light entities | yes |
| HVAC | handler-backed `publishChannelState`, `subscribeChannel` | action, mode and generic or heat/cool setpoints, including off state | execute action and setpoints, including active `toggle` | heat, cool and heat-cool thermostat variants | yes (heat, cool, scheduled heat-cool, off; discovery) |
| Binary sensor | `publishChannelState` | open/closed or ON/OFF depending on function | read-only | broad function matrix | yes (door open/closed and flood ON) |
| Action trigger | no state publish, HA discovery tests | no regular state topic | no channel subscription | capabilities and trigger variants | no public traffic to capture |
| Electricity meter | `publishExtendedChannelState` in `mqtt_publish_tests.cpp` | total forward/reverse and balanced energy, phase sequence and angles, all optional per-phase measurements | read-only | discovery parameter matrix | yes (global and full per-phase values) |
| Cleanup / retained deletion | pair transition and unavailable-channel tests | empty retained state topics | — | empty retained discovery configs | yes (one mixed state/cleanup scenario) |
| Prefix / topic utility | prefix and `MqttTopic` unit tests | construction only | construction only | — | not applicable |
| Handler registry | registry lifecycle tests | no traffic | no traffic | — | not applicable |

## Command payload evidence

This maintainer-facing table records payloads exercised by existing
`processData` tests. The same evidence remains available in `metadata.json` as
`tested_payloads` and `confirmed_by_tests`.

| Topic | Tested payloads | Parser test |
|---|---|---|
| `set/on` | `true`, `false`, `1`, `0`, `YES`, `FALSE` | `MqttProcessDataTests.relaySetOnTests` |
| relay `execute_action` | `turn_on`, `turn_off`, `toggle` | `MqttProcessDataTests.executeActionTests` |
| `set/brightness` | `55`, `77` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/color` | `1,2,3`, `9,8,7` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/color_brightness` | `44`, `66` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/closing_percentage` | `10` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/tilt` | `20` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/temperature_setpoint` | `19.5` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/temperature_setpoint_heat` | `18.5` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `set/temperature_setpoint_cool` | `22.5` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| `execute_action/rgb` | `turn_off` | `MqttChannelDispatchTests.processDataCoversControlTypes` |
| HVAC `execute_action` | `turn_on`, `turn_off`, `off`, `toggle`, `auto`, `heat`, `cool`, `heat_cool` | `MqttChannelDispatchTests.processDataCoversHvacActionsAndBoundaries`, `MqttChannelDispatchTests.processDataHvacToggleTurnsOffActiveHvac` |

## Missing or deliberately deferred scenarios

- Electricity meter scenarios now capture global reverse/balanced energy,
  phase sequence and angles, plus all optional per-phase measurements.
- HVAC command parsing covers every documented action, both `toggle` branches,
  generic heat setpoint routing, and rejected invalid payloads.
- HVAC state capture now covers heat, cool, scheduled heat-cool, off, and
  missing setpoints.
- Command payload examples come from `processData` tests, whereas the recorder
  observes broker-to-device availability at `subscribeImp`. Metadata links the
  tested values to the corresponding parser tests.
- Home Assistant Discovery capture covers HVAC heat, cool and heat-cool,
  impulse and switch relays, cover-function variants, binary sensors, all
  action-trigger capabilities, thermometer/humidity sensors, lighting
  controllers, and electricity meters.
- Invalid input, no-op, and broad retained cleanup paths remain intentionally
  outside public scenarios.

The generated reference is coverage-driven: absence from the generated files
does not prove that production code cannot emit a topic.
