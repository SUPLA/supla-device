# MQTT topics

Languages: **English** · [Polski](topics.pl.md)

This document describes the MQTT topics exposed by SUPLA devices 
operating in MQTT mode.

`{prefix}` means `[custom-prefix/]supla/devices/{hostname}`. 
`{channel}` is the channel number. 
`{phase}` is the electricity-meter phase number.

Topics under **Subscribed topics** accept commands sent to the device. 
Topics under **Published topics** contain states and measurements 
published by the device.

The exact set of available topics depends on the device configuration, 
channel types, channel functions, and supported measurements.

## Channel types and functions

[Device](#device) · [Relay](#relay) · [Roller shutter](#roller-shutter) · [Dimmer](#dimmer) · [RGB controller](#rgb-controller) · [Dimmer and RGB controller](#dimmer-and-rgb-controller) · [Thermometer](#thermometer) · [Humidity and temperature sensor](#humidity-and-temperature-sensor) · [HVAC](#hvac) · [Electricity meter](#electricity-meter) · [Binary sensor](#binary-sensor) · [Channel](#channel)

## Device

### Published topics

#### Connection state

- Topic: `{prefix}/state/connected`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: All MQTT-enabled devices.

MQTT availability state of the device.

Example: `true`

#### Connection uptime

- Topic: `{prefix}/state/connection_uptime`
- Payload type: `integer`
- Unit: `s`
- QoS: `0`
- Retain: `false`
- Availability: Devices that do not use sleeping mode.

Time since the current connection was established.

Example: `0`

#### IP address

- Topic: `{prefix}/state/ip`
- Payload type: `string`
- QoS: `0`
- Retain: `false`
- Availability: Published during MQTT registration.

IPv4 address reported by the active network interface.

Example: `192.0.2.10`

#### MAC address

- Topic: `{prefix}/state/mac`
- Payload type: `string`
- QoS: `0`
- Retain: `false`
- Availability: Published during MQTT registration.

Main device MAC address in colon-separated hexadecimal form.

Example: `01:02:03:04:05:AB`

#### Wi-Fi RSSI

- Topic: `{prefix}/state/rssi`
- Payload type: `integer`
- Unit: `dBm`
- QoS: `0`
- Retain: `false`
- Availability: Wi-Fi interfaces that expose RSSI.

Received Wi-Fi signal strength reported by the device.

Example: `-67`

#### Device uptime

- Topic: `{prefix}/state/uptime`
- Payload type: `integer`
- Unit: `s`
- QoS: `0`
- Retain: `false`
- Availability: All MQTT-enabled devices.

Device uptime in seconds.

Example: `0`

#### Wi-Fi signal strength

- Topic: `{prefix}/state/wifi_signal_strength`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: Wi-Fi interfaces that expose signal strength.

Normalized Wi-Fi signal strength reported by the device.

Example: `73`

## Relay

Channel type: `SUPLA_CHANNELTYPE_RELAY`

Channel functions: `SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND`, `SUPLA_CHANNELFNC_CONTROLLINGTHEGATE`, `SUPLA_CHANNELFNC_LIGHTSWITCH`, `SUPLA_CHANNELFNC_POWERSWITCH`

### Subscribed topics

#### Execute relay action

- Topic: `{prefix}/channels/{channel}/execute_action`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Availability: Power-switch relay channels.

Switches the relay on, off, or toggles its current state. Matching is case-insensitive.

Example: `turn_on`

#### Set closing percentage

- Topic: `{prefix}/channels/{channel}/set/closing_percentage`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: Roller shutters and compatible cover functions.

Sets the cover position, where 0 is fully open and 100 is fully closed.

Example: `10`

#### Set relay state

- Topic: `{prefix}/channels/{channel}/set/on`
- Payload type: `string`
- Allowed values: `true`, `1`, `yes`, `false`, `0`, `no`
- QoS: `0`
- Retain: `false`
- Availability: Switch-like relay functions.

Changes a switch-like relay output. Matching is case-insensitive.

The current parser treats any value other than `1`, `yes`, or `true` as OFF. Integrations should use only the documented values.

Example: `true`

#### Set tilt

- Topic: `{prefix}/channels/{channel}/set/tilt`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: Cover functions with tilt support.

Sets the slat tilt percentage.

Example: `20`

### Published topics

#### Calibration state

- Topic: `{prefix}/channels/{channel}/state/is_calibrating`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: Roller shutters and compatible cover functions.

Indicates whether cover calibration is in progress.

Example: `false`

#### Impulse relay state

- Topic: `{prefix}/channels/{channel}/state/on`
- Payload type: `string`
- Allowed values: `closed`
- QoS: `0`
- Retain: `true`
- Availability: Power-switch and light-switch relay channels.

Idle state reported by an impulse gate, garage-door, or lock relay.

Examples: `closed`, `false`, `true`

#### Closing percentage

- Topic: `{prefix}/channels/{channel}/state/shut`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: Roller shutters and compatible cover functions.

Current cover position, where 0 is fully open and 100 is fully closed.

Example: `40`

#### Tilt position

- Topic: `{prefix}/channels/{channel}/state/tilt`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: Cover functions with tilt support.

Current slat tilt position.

Example: `60`

## Roller shutter

Channel type: `SUPLA_CHANNELTYPE_RELAY`

Channel function: `SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER`

### Subscribed topics

#### Execute roller shutter action

- Topic: `{prefix}/channels/{channel}/execute_action`
- Payload type: `string`
- Allowed values: `reveal`, `shut`, `stop`, `calibrate`, `recalibrate`
- QoS: `0`
- Retain: `false`
- Availability: Roller shutters and compatible cover functions.

Opens, closes, stops, or recalibrates the roller shutter. Matching is case-insensitive.

Example: `reveal`

#### Set closing percentage

- Topic: `{prefix}/channels/{channel}/set/closing_percentage`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: Roller shutters and compatible cover functions.

Sets the cover position, where 0 is fully open and 100 is fully closed.

Example: `10`

### Published topics

#### Calibration state

- Topic: `{prefix}/channels/{channel}/state/is_calibrating`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: Roller shutters and compatible cover functions.

Indicates whether cover calibration is in progress.

Example: `false`

#### Closing percentage

- Topic: `{prefix}/channels/{channel}/state/shut`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: Roller shutters and compatible cover functions.

Current cover position, where 0 is fully open and 100 is fully closed.

Example: `33`

## Dimmer

Channel type: `SUPLA_CHANNELTYPE_DIMMER`

Channel function: `SUPLA_CHANNELFNC_DIMMER`

### Subscribed topics

#### Execute dimmer action

- Topic: `{prefix}/channels/{channel}/execute_action`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Availability: Dimmer-capable channels.

Switches the dimmer on, off, or toggles its current state. Matching is case-insensitive.

Example: `turn_on`

#### Set brightness

- Topic: `{prefix}/channels/{channel}/set/brightness`
- Payload type: `integer`
- Range: `0..100`
- QoS: `0`
- Retain: `false`
- Availability: Dimmer-capable channels.

Sets the channel brightness.

Example: `55`

### Published topics

#### Brightness state

- Topic: `{prefix}/channels/{channel}/state/brightness`
- Payload type: `integer`
- Range: `0..100`
- QoS: `0`
- Retain: `true`
- Availability: Dimmer-capable channels.

Current dimmer brightness.

Example: `42`

#### On state

- Topic: `{prefix}/channels/{channel}/state/on`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: Dimmer-capable channels.

Current on/off state.

Example: `true`

## RGB controller

Channel type: `SUPLA_CHANNELTYPE_RGBLEDCONTROLLER`

Channel function: `SUPLA_CHANNELFNC_RGBLIGHTING`

### Subscribed topics

#### Execute RGB action

- Topic: `{prefix}/channels/{channel}/execute_action`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Availability: RGB-capable channels.

Switches the RGB output on, off, or toggles its current state. Matching is case-insensitive.

Example: `turn_on`

#### Set RGB color

- Topic: `{prefix}/channels/{channel}/set/color`
- Payload type: `string`
- QoS: `0`
- Retain: `false`
- Availability: RGB-capable channels.

Sets an RGB color as comma-separated red, green, and blue values in the 0..255 range.

Example: `1,2,3`

#### Set color brightness

- Topic: `{prefix}/channels/{channel}/set/color_brightness`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: RGB-capable channels.

Sets RGB color brightness without changing the color.

Example: `44`

### Published topics

#### RGB color

- Topic: `{prefix}/channels/{channel}/state/color`
- Payload type: `string`
- QoS: `0`
- Retain: `true`
- Availability: RGB-capable channels.

Current RGB color as comma-separated red, green, and blue values.

Example: `1,2,3`

#### Color brightness

- Topic: `{prefix}/channels/{channel}/state/color_brightness`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: RGB-capable channels.

Current RGB color brightness.

Example: `4`

#### On state

- Topic: `{prefix}/channels/{channel}/state/on`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: RGB-capable channels.

Current on/off state.

Example: `true`

## Dimmer and RGB controller

Channel type: `SUPLA_CHANNELTYPE_DIMMERANDRGBLED`

Channel function: `SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING`

### Subscribed topics

#### Execute dimmer action

- Topic: `{prefix}/channels/{channel}/execute_action/dimmer`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Availability: Combined dimmer and RGB channels.

Controls the dimmer part of a combined dimmer and RGB channel.

Example: `turn_on`

#### Execute RGB action

- Topic: `{prefix}/channels/{channel}/execute_action/rgb`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Availability: Combined dimmer and RGB channels.

Controls the RGB part of a combined dimmer and RGB channel.

Example: `turn_off`

#### Set brightness

- Topic: `{prefix}/channels/{channel}/set/brightness`
- Payload type: `integer`
- Range: `0..100`
- QoS: `0`
- Retain: `false`
- Availability: Dimmer-capable channels.

Sets the channel brightness.

Example: `55`

#### Set RGB color

- Topic: `{prefix}/channels/{channel}/set/color`
- Payload type: `string`
- QoS: `0`
- Retain: `false`
- Availability: RGB-capable channels.

Sets an RGB color as comma-separated red, green, and blue values in the 0..255 range.

Example: `1,2,3`

#### Set color brightness

- Topic: `{prefix}/channels/{channel}/set/color_brightness`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `false`
- Availability: RGB-capable channels.

Sets RGB color brightness without changing the color.

Example: `44`

### Published topics

#### Brightness state

- Topic: `{prefix}/channels/{channel}/state/brightness`
- Payload type: `integer`
- Range: `0..100`
- QoS: `0`
- Retain: `true`
- Availability: Dimmer-capable channels.

Current dimmer brightness.

Example: `8`

#### RGB color

- Topic: `{prefix}/channels/{channel}/state/color`
- Payload type: `string`
- QoS: `0`
- Retain: `true`
- Availability: RGB-capable channels.

Current RGB color as comma-separated red, green, and blue values.

Example: `5,6,7`

#### Color brightness

- Topic: `{prefix}/channels/{channel}/state/color_brightness`
- Payload type: `integer`
- Range: `0..100`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: RGB-capable channels.

Current RGB color brightness.

Example: `11`

#### Dimmer on state

- Topic: `{prefix}/channels/{channel}/state/dimmer/on`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: Combined dimmer and RGB channels.

Current on/off state of the dimmer part.

Example: `true`

#### RGB on state

- Topic: `{prefix}/channels/{channel}/state/rgb/on`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: Combined dimmer and RGB channels.

Current on/off state of the RGB part.

Example: `true`

## Thermometer

Channel type: `SUPLA_CHANNELTYPE_THERMOMETER`

Channel function: `SUPLA_CHANNELFNC_NONE`

### Published topics

#### Temperature

- Topic: `{prefix}/channels/{channel}/state/temperature`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `true`
- Availability: Temperature-capable sensors with a valid reading.

Current temperature reading.

Example: `21.50`

## Humidity and temperature sensor

Channel type: `SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR`

Channel function: `SUPLA_CHANNELFNC_NONE`

### Published topics

#### Humidity

- Topic: `{prefix}/channels/{channel}/state/humidity`
- Payload type: `number`
- Unit: `%`
- QoS: `0`
- Retain: `true`
- Availability: Humidity and temperature sensors with a valid humidity reading.

Current relative humidity.

Example: `55.00`

#### Temperature

- Topic: `{prefix}/channels/{channel}/state/temperature`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `true`
- Availability: Temperature-capable sensors with a valid reading.

Current temperature reading.

Example: `21.50`

## HVAC

Channel type: `SUPLA_CHANNELTYPE_HVAC`

Channel functions: `SUPLA_CHANNELFNC_HVAC_THERMOSTAT`, `SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL`

### Subscribed topics

#### Execute HVAC action

- Topic: `{prefix}/channels/{channel}/execute_action`
- Payload type: `string`
- Allowed values: `turn_on`, `turn_off`, `off`, `toggle`, `auto`, `heat`, `cool`, `heat_cool`
- QoS: `0`
- Retain: `false`
- Availability: Heat-cool HVAC thermostat channels.

Changes the operating state or mode of a heat-cool thermostat. Matching is case-insensitive.

Example: `turn_on`

#### Set temperature setpoint

- Topic: `{prefix}/channels/{channel}/set/temperature_setpoint`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `false`
- Availability: HVAC thermostat channels.

Sets the active heating or cooling setpoint according to the HVAC channel configuration.

Example: `19.5`

#### Set cooling setpoint

- Topic: `{prefix}/channels/{channel}/set/temperature_setpoint_cool`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `false`
- Availability: HVAC channels with a cooling setpoint.

Sets the cooling temperature setpoint.

Example: `22.5`

#### Set heating setpoint

- Topic: `{prefix}/channels/{channel}/set/temperature_setpoint_heat`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `false`
- Availability: HVAC channels with a heating setpoint.

Sets the heating temperature setpoint.

Example: `18.5`

### Published topics

#### HVAC action

- Topic: `{prefix}/channels/{channel}/state/action`
- Payload type: `string`
- Allowed values: `off`, `idle`, `heating`, `cooling`
- QoS: `0`
- Retain: `true`
- Availability: HVAC channels.

Current operating action of the HVAC controller.

Examples: `cooling`, `heating`, `idle`, `off`

#### HVAC mode

- Topic: `{prefix}/channels/{channel}/state/mode`
- Payload type: `string`
- Allowed values: `off`, `auto`, `heat`, `cool`, `heat_cool`
- QoS: `0`
- Retain: `true`
- Availability: HVAC channels.

Current HVAC operating mode.

Examples: `auto`, `cool`, `heat`, `heat_cool`, `off`

#### Temperature setpoint

- Topic: `{prefix}/channels/{channel}/state/temperature_setpoint`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `true`
- Availability: HVAC functions using one setpoint.

Current single HVAC temperature setpoint.

Examples: `19.50`, `23.00`

#### Cooling setpoint

- Topic: `{prefix}/channels/{channel}/state/temperature_setpoint_cool`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `true`
- Availability: HVAC functions with separate heating and cooling setpoints.

Current cooling temperature setpoint.

Example: `23.00`

#### Heating setpoint

- Topic: `{prefix}/channels/{channel}/state/temperature_setpoint_heat`
- Payload type: `number`
- Unit: `°C`
- QoS: `0`
- Retain: `true`
- Availability: HVAC functions with separate heating and cooling setpoints.

Current heating temperature setpoint.

Example: `21.00`

## Electricity meter

Channel type: `SUPLA_CHANNELTYPE_ELECTRICITY_METER`

Channel function: `SUPLA_CHANNELFNC_ELECTRICITY_METER`

### Published topics

#### Current phase sequence

- Topic: `{prefix}/channels/{channel}/state/current_phase_sequence_clockwise`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting current phase sequence.

Whether the current phase sequence is clockwise.

Example: `false`

#### Phase current

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/current`
- Payload type: `number`
- Unit: `A`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase current.

Current measured on one phase.

Examples: `1.234`, `0.000`

#### Phase frequency

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/frequency`
- Payload type: `number`
- Unit: `Hz`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase frequency.

Frequency measured on one phase.

Example: `50.00`

#### Phase angle

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/phase_angle`
- Payload type: `number`
- Unit: `°`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase phase angle.

Phase angle measured on one phase.

Examples: `12.3`, `0.0`

#### Active power

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/power_active`
- Payload type: `number`
- Unit: `W`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase active power.

Current active power for one phase.

Examples: `200.000`, `0.000`

#### Apparent power

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/power_apparent`
- Payload type: `number`
- Unit: `VA`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase apparent power.

Current apparent power for one phase.

Examples: `0.000`, `0.300`

#### Phase power factor

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/power_factor`
- Payload type: `number`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase power factor.

Power factor measured on one phase.

Examples: `0.99`, `0.00`

#### Reactive power

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/power_reactive`
- Payload type: `number`
- Unit: `var`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase reactive power.

Current reactive power for one phase.

Examples: `0.000`, `0.100`

#### Phase forward active energy

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/total_forward_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase forward active energy.

Imported active energy for one phase.

Examples: `0.0100`, `0.0000`

#### Phase forward reactive energy

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/total_forward_reactive_energy`
- Payload type: `number`
- Unit: `kvarh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase forward reactive energy.

Imported reactive energy for one phase.

Examples: `0.0300`, `0.0000`

#### Phase reverse active energy

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/total_reverse_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase reverse active energy.

Exported active energy for one phase.

Examples: `0.0200`, `0.0000`

#### Phase reverse reactive energy

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/total_reverse_reactive_energy`
- Payload type: `number`
- Unit: `kvarh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase reverse reactive energy.

Exported reactive energy for one phase.

Examples: `0.0400`, `0.0000`

#### Phase voltage

- Topic: `{prefix}/channels/{channel}/state/phases/{phase}/voltage`
- Payload type: `number`
- Unit: `V`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting per-phase voltage.

Voltage measured on one phase.

Examples: `230.00`, `0.00`

#### Total forward active energy

- Topic: `{prefix}/channels/{channel}/state/total_forward_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting forward active energy.

Total imported active energy across all phases.

Example: `0.0100`

#### Total balanced forward active energy

- Topic: `{prefix}/channels/{channel}/state/total_forward_balanced_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting balanced forward active energy.

Total imported active energy balanced across phases.

Example: `0.0300`

#### Total reverse active energy

- Topic: `{prefix}/channels/{channel}/state/total_reverse_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting reverse active energy.

Total exported active energy across all phases.

Example: `0.0200`

#### Total balanced reverse active energy

- Topic: `{prefix}/channels/{channel}/state/total_reverse_balanced_active_energy`
- Payload type: `number`
- Unit: `kWh`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting balanced reverse active energy.

Total exported active energy balanced across phases.

Example: `0.0400`

#### Voltage phase angle 12

- Topic: `{prefix}/channels/{channel}/state/voltage_phase_angle_12`
- Payload type: `number`
- Unit: `°`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting voltage phase angles.

Voltage phase angle between phases 1 and 2.

Example: `120.0`

#### Voltage phase angle 13

- Topic: `{prefix}/channels/{channel}/state/voltage_phase_angle_13`
- Payload type: `number`
- Unit: `°`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting voltage phase angles.

Voltage phase angle between phases 1 and 3.

Example: `240.0`

#### Voltage phase sequence

- Topic: `{prefix}/channels/{channel}/state/voltage_phase_sequence_clockwise`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `false`
- Availability: Electricity meters reporting voltage phase sequence.

Whether the voltage phase sequence is clockwise.

Example: `true`

## Binary sensor

Channel type: `SUPLA_CHANNELTYPE_BINARYSENSOR`

Channel functions: `SUPLA_CHANNELFNC_FLOOD_SENSOR`, `SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR`

### Published topics

#### Binary sensor state

- Topic: `{prefix}/channels/{channel}/state`
- Payload type: `string`
- Allowed values: `open`, `closed`, `ON`, `OFF`
- QoS: `0`
- Retain: `true`
- Availability: Binary sensor channels with a configured function.

Current binary sensor state; vocabulary depends on the channel function.

Examples: `ON`, `closed`, `open`

## Channel

Channel type: `channel`

### Published topics

#### Channel availability

- Topic: `{prefix}/channels/{channel}/state/available`
- Payload type: `boolean`
- Allowed values: `true`, `false`
- QoS: `0`
- Retain: `true`
- Availability: All MQTT-enabled channels.

Current availability of the channel resource.

Examples: `false`, `true`
