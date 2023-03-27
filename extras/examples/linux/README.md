# supla-device for Linux

This application can be run on a Linux platform and work as a background
service.

supla-device is an application which provides channels in Supla
(see https://www.supla.org), which can be controlled and viewed by
Supla application on mobile phone, or via web interface on
https://cloud.supla.org.

# Build

## Install dependencies

For Debian based distributions:

    sudo apt install git libssl-dev build-essential libyaml-cpp-dev cmake

## Get supla-device sources

    git clone https://github.com/SUPLA/supla-device.git

## Compilation

We use CMake to control build process.
Please adjust SUPLA_DEVICE_PATH to the actual path where supla-device repository
was cloned.

    export SUPLA_DEVICE_PATH=~/supla-device
    cd supla-device/extras/examples/linux
    mkdir build
    cd build
    cmake ..
    make

If you want to speed up compilation you can call `make -j5` instead of `make`.
It will run 5 parallel compilation jobs. However it is not recommended to use
on a PC with low RAM.

It should produce `supla-device-linux` binary file. Check if it is working:

    ./supla-device-linux --version

# Usage

Currently there is no automated installation available. So please follow below
instructions.

## Working modes

supla-device may work in 3 modes:
1. "normal" - default mode, when you call `./supla-device-linux` from command
line. In this mode logs are printed to console (standard output). Application
working directory depends on current working directory in a console.
2. "daemon" - can be started by calling `./supla-device-linux -d`. Application
is forked and runs in background. Logs are routed to syslog
(see /var/log/syslog). Current working directory is changed to `/`.
supla-device doesn't create PID file.
3. "service" - can be started by calling `./supla-device-linux -s`. Logs are
routed to syslog and current working directory is changed to `/`
(as in daemon mode). However separate process isn't forked and application
runs in foreground.

## Logs, problems, bugs, help

In case of any problem, please check first logs from supla-device. By default
they are printed on a console, however in daemon/service mode those can be
found in syslog:

    tail -n 100 /var/log/syslog | grep supla-device-linux

This command will show last 100 entries in syslog and filter it for
supla-device-linux string.

You can change log level by adding `-D` (debug) or `-V` (verbose debug) to your
command line, or in `supla-device.yaml` config file.

If you need further help, please ask on
[forum](https://en-forum.supla.org/viewforum.php?f=7) (or use any other
language variant on forum, however majority of Supla forum users speaks
English and Polish). For Polish language, you may ask questions
[here](https://forum.supla.org/viewforum.php?f=18).

If you encounter any bug, or have some new feature proposal, please report it
either on forum (links above) or as a
[Github issue](https://github.com/SUPLA/supla-device/issues).

Feel free to contribute.

## Config file location

supla-device use supla-device.yaml file for configuration. By default it checks
`etc/supla-device.yaml` relative path. So it will look in current folder when
run in "normal" mode, however in "daemon" or "service" mode, it will look
for a config file in `/etc/supla-device.yaml`.

You can specify your own config file:

    ./supla-device-linux -c /path/to/your/file/supla-cfg.yml

## GUID, AUTHKEY, last_state.txt

GUID and AUTHKEY is automatically generated (if missing) and stored in location:
`var/lib/supla-device/guid_auth.yaml`. Directory may be modified, but file
name can't.
Please make sure that user, under which supla-device will be started, has write
access to the `var/lib/supla-device` location. I.e. by calling:

    sudo mkdir -p /var/lib/supla-device
    sudo chown supla_user /var/lib/supla-device

Adjust "supla_user" to your user name.

`last_state.txt` file contain last runtime "last state" log (similar to
"last state" log available on web interface for i.e. ESP8266 devices). It is
rewritten on each startup.

Application will fail not start when it doesn't have access to this location.

Location for those files may be modified by providing providing
`state_files_path` in you config YAML file.

## Example supla-device.yaml config file

Example config file is available in `extras/examples/linux/supla-device.yaml`.
Please copy it and adjust to your needs.
If you are not familiar with YAML format, please check some examples online
before you start. Data is case sensitive and indentation is important.

## Config file parameters

### Generic configuration

#### Parameter `name`

Device name with which it registers to Supla Server.
Parameter is optional.

Example:

    name: My fancy device

#### Parameter `log_level`

Use it to change log level to different value.
Parameter is optional. Default log level is `info`.
Allowed values: `debug`, `verbose`

Example:

    log_level: debug

#### Parameter `state_files_path`

Defines location where supla-device will read/write GUID, AUTHKEY and
`last_state.txt`.
Parameter is optional. Default value is: `var/lib/supla-device` (relative path).
Allowed values: any valid relative or absolute path where supla-device will have
proper rights to write and read files.
Please put path in double quotes.

Example:

    state_files_path: "/home/supla_user/.supla-device"

#### Parameter `security_level`

Defines if Supla server ceritficate should be validated against root CA.
Values:
 - 0 - Supla root CA is used for validation (default) - not implemented yet
 - 2 - skip certificate validation - not recommended, however this is the only
 option for now.
Parameter is optional. Default value 0.

Example:

    security_level: 2

### Supla server connection

Below parameters should be defined under `supla` key (as in examples below).

#### Parameter `server`

Defines Supla server address.
Mandatory.

#### Parameter `port`

Defines Supla server port to which device should connect to. This application
use only TLS encrypted connection, so by default port 2016 is used in Supla
for this purpose.
Parameter is optional - default value: 2016.

#### Parameter `mail`

Defines mail address which is used for user account on Supla Server.
Mandatory.

Example:

    supla:
      server: svr12.supla.org
      port: 2016
      mail: user@my_mail_server.com

# Supla channels configuration

Channels are defined as YAML array under `channels` key. Each array element
starts with `-` and should have proper indentation.

Order of channels is important. First channel will get number 0, second 1, etc.
and will be registered in Supla server with those numbers. In Supla,
device is not allowed to change order of channels, nor to change channel type,
nor to remove channel. If you do so, device will not be able to register on
Supla server and will have "Channel conflict" error (visible in logs and in
`last_state.txt` file). In such case you will have to remove device from Supla
Cloud with all measurement history and configuration, and register it again.

It is ok to add channels later at the end of the list.

Currently only limited number of channels are supported. Please let us know
if you need something more.

Supported channel types:
* `VirtualRelay` - related class `Supla::Control::VirtualRelay`
* `CmdRelay` - related class `Supla::Control::CmdRelay`
* `Fronius` - related class `Supla::PV::Fronius`
* `Afore` - related class `Supla::PV::Afore`
* `ThermometerParsed` - related class `Supla::Sensor::ThermometerParsed`
* `ThermHygroMeterParsed` - related class `Supla::Sensor::ThermHygroMeterParsed`
* `ImpulseCounterParsed` - related class `Supla::Sensor::ImpulseCounterParsed`
* `ElectricityMeterParsed` - related class `Supla::Sensor::ElectricityMeterParsed`
* `BinaryParsed` - related class `Supla::Sensor::BinaryParsed`
* `ActionTriggerParsed` - related class `Supla::Control::ActionTriggerParsed`
* `HumidityParsed` - related class `Supla::Sensor::HumidityParsed
* `PressureParsed` - related class `Supla::Sensor::PressureParsed`
* `WindParsed` - related class `Supla::Sensor::WindParsed`
* `RainParsed` - related class `Supla::Sensor::RainParsed`

Example channels configuration (details are exaplained later):

    channels:
      - type: VirtualRelay
        name: vr1 # optional, can be used as reference for adding actions (TBD)
        initial_state: on

      - type: VirtualRelay
        name: vr2
        initial_state: restore

    # CmdRelay with state kept in memory and stored in storage
      - type: CmdRelay
        name: command_relay_1
        initial_state: restore
        cmd_on: "echo 1 > command_relay_1.out"
        cmd_off: "echo 0 > command_relay_1.out"

    # CmdRelay with state read from data source
      - type: CmdRelay
        name: command_relay_2
        cmd_on: "echo 1 > command_relay_1.out"
        cmd_off: "echo 0 > command_relay_1.out"
        source:
          type: Cmd
          command: "cat in_r2.txt"
        parser:
          type: Simple
          refresh_time_ms: 1000
        state: 0

      - type: Fronius
        ip: 192.168.1.7
        port: 80
        device_id: 1

      - type: Afore
        ip: 192.168.1.17
        port: 80
    # login_and_password is base64 encoded "login:password"
    # You can use any online base64 encoder to convert your login and password,
    # i.e. https://www.base64encode.org/
    login_and_password: "bG9naW46cGFzc3dvcmQ="

      - type: ThermometerParsed
        name: t1
        temperature: 0
        multiplier: 1
        parser:
          name: parser_1
          type: Simple
          refresh_time_ms: 200
        source:
          name: s1
          type: File
          file: temp.txt

      - type: ThermometerParsed
        name: t2
        temperature: 3
        multiplier: 1
        parser:
          use: parser_1

      - type: ImpulseCounterParsed
        name: i1
        counter: total_m3
        multiplier: 1000
        source:
          type: File
          file: "/var/log/wmbusmeters/meter_readings/water.json"
        parser:
          type: Json

      - type: ElectricityMeterParsed
        parser:
          type: Json
        source:
          type: File
          file: tauron.json
        phase_1:
          - voltage: voltage_at_phase_1_v
            multiplier: 1
          - fwd_act_energy: total_energy_consumption_tariff_1_kwh
            multiplier: 1
          - rvr_act_energy: total_energy_production_tariff_1_kwh
            multiplier: 1
          - power_active: current_power_consumption_kw
            multiplier: 1000
          - rvr_power_active: current_power_production_kw
            multiplier: 1000
        phase_2:
          - voltage: voltage_at_phase_2_v
            multiplier: 1
          - fwd_act_energy: total_energy_consumption_tariff_2_kwh
            multiplier: 1
          - rvr_act_energy: total_energy_production_tariff_2_kwh
            multiplier: 1
        phase_3:
          - voltage: voltage_at_phase_3_v
            multiplier: 1

      - type: ThermometerParsed
        temperature: 0
        source:
          type: Cmd
      # call sensors, take line with word "temp3", split it on ":" and return second
      # value
          command: "sensors | grep temp3 | cut -d : -f2"
        parser:
          type: Simple

      - type: BinaryParsed
        state: 1
        parser:
          use: parser_1

      - type: ThermHygroMeterParsed
        name: th1
        source:
          type: File
      # use file "temp_humi.txt" from current folder
          file: "temp_humi.txt"
        parser:
          type: Simple
          refresh_time_ms: 200
      # temperature is read from first line of txt file
        temperature: 0
      # humidity is read from second line of txt file
        humidity: 1
        multiplier_temp: 1
        multiplier_humi: 1
        battery_level: 2
        multiplier_battery_level: 100

There are some new classes (compared to standard non-Linux supla-device) which
names end with "Parsed" word. In general, those channels use `parser` and
`source` functions to get some data from your computer and put it to that
channel.

More examples can be found in subfolders of `extras/examples/linux`.

### VirtualRelay

`VirtualRelay` is pretending to be a relay channel in Supla. You can turn it on
and off from Supla App, etc.

It is virtual, because it doesn't control anything - it just keeps state that
was set on it.

There are two optional parameters:
`name` - name of channel in YAML file - it doesn't have any functional meaning
so far.
`initial_state` - allows to define what state should be set on relay when
supla-device application is started. Following values are allowed:
on, off, restore.
"off" is default value.
"restore" will use state storage file to keep and restore relay state.

### CmdRelay

`CmdRelay` is pretending to be a relay channel in Supla. It is very similar to
`VirtualRelay`, however additionally it allows to configure Linux command to
be executed on every turn on/off action.

`CmdRelay` accepts the same parameters as `VirtualRelay`. Additionally it supports
two extra configuration options:
`cmd_on` - command to be exectued on turn on.
`cmd_off` - command to be executed on turn off.

When `CmdRelay` is added without `state` parameter, then it will use internal
memory to keep it's state, which will be always consistent with last executed
action on relay channel. Such state can be saved to Storage.

Another option for `CmdRelay` is to define `state` parameter. When `state`
parameter is defined, it require to use `Parser` instance (and underlying `Source`)
which is used to read state of this relay channel. It works exactly the same as
for `BinaryParsed` channel. So you can define data source as file or command and
use any available `Parser` to read your relay state. Please remember to keep
state refresh rate at reasonable level (i.e. fetching data remotly every
100 ms may not be the best idea :) ).

Parameter `offline_on_invalid_state` set to `true` will change channel to "offline"
when its state is invalid (i.e. source file wasn't modfified for a long time, or
value was set to -1).

Paramter `state_on_values` allows to define array of integers which are interpreted
as state "on". I.e. `state_on_values = [3, 4, 5]` will set channel to "on"
when state is 3, 4 or 5. Otherwise it will set channel to "off" with exception to
value -1 which is used as invalid state.

Parameter `action_trigger` allows to use `ActionTriggerParsed` channel to send actions
to Supla server depending on channel state (or value). Example:

    action_trigger:
      use: at1
      on_state: [1, 0]
      on_state: [2, 1]

Exact values and configuration is exaplained in `ActionTriggerParsed` section.
Parameter `use: at1` indicates which `ActionTriggerParsed` instance should be used
to send actions. "at1" is a name of `ActionTriggerParsed` instance.

## Parsed channel `source` parameter

`source` defines from where supla-device will get data for `parser` to parsed
channel. It should be defined as a subelement of a channel.

`source` have one common mandatory parameter `type` which defines type
of used source. There is also optional `name` parameter. If you name your
source, then it can be reused for multiple parsers.

There are two supported parser types:
1. `File` - use file as an input. File name is provided by `file` parameter and
additionally you can define `expiration_time_sec` parameter. If last modification
time of a file is older than `expiration_time_sec` then this source will be
considered as invalid. `expiration_time_sec` is by default set to 10 minutes. 
In order to disable time expiration check, please set `expiration_time_sec` to 0.
2. `Cmd` - use Linux command line as an input. Command is provided by `commonad`
field.

If source was already defined earlier and you want to reuse it, you can specify
`use` parameter with proper name of previously defined source. When `use`
parameter is used, then no other source configuration parameters are allowed.

## Parsed channel `parser` parameter

Parser takes text input from previously defined `source` and converts it to
value which can be used for a parsed channel value.

There are two parsers defined:
1. `Simple` - it takes input from source and try to convert each line of text
to a floating point number. Value from each line can be referenced later by
using line index number (index counting starts with 0). I.e. please take a look
at `t1` channel above.
2. `Json` - it takes input from source and parse it as JSON format. Values can
be referenced in parsed channel by JSON key name or by JSON pointer and each
value is converted to a floating point number. I.e. please check `i1`
channel above. More details about parsing JSON can be found in JSON parser
section of this document.

Type of a parser is selected with a `type` parameter. You can provide a name for
your parser with `name` parameter (named parsers can be reused for different
channels). Additionally parsers allow to configure `refresh_time_ms` parameter
which provides period of time in ms, how often parser will try to refresh data
from source. Please keep in mind that it doesn't override refresh times which
are used in channel itself. I.e. thermometers are refreshed every 10 s, while
binary senosors are refereshed every 100 ms. Default refresh time for parser is
set to 5 s, so in that case thermomenter value will update every 10 s, and
binary sensor every 5 s. If you'll set `refresh_time_ms` to `200`, then
thermometer value will still refresh every 10 s, but binary sensor value will
update every 200 ms.

If parser was already defined earlier, you can reuse it by providing `use`
parameter with a parser name.

### JSON parser
JSON parser takes input from source and parse it as JSON format. Values can
be referenced in parsed channel by JSON key name or by JSON pointer and each
value is converted to a floating point number.

JSON pointer is specified in [RFC6901](https://www.rfc-editor.org/rfc/rfc6901).

I.e. consider following JSON:

    {
      "my_temperature": 23.5,
      "measurements": [
        {
          "name": "humidity",
          "value": 84.1
        },
        {
          "name": "pressure",
          "value": 1023
        }
      ]
    }

Temperature value can be accessed by providing key name, because it is directly
under the root of JSON structure:

    temperature: "my_temperature"

Alternatively you can use JSON pointer to access the same value:

    temperature: "/my_temperature"

All keys are considered as JSON pointer when they start with "/", otherwise
keys are expected to be name of parameter in the root structure.

In order to access humidity or pressure values, you have to specify JSON
pointer, becuase they are not in the root:

    pressure: "/measurements/1/value"
    humidity: "/measurements/0/value"

If you use such JSON with arrays as a source, please make sure that order of
array elements will not change, because it will change JSON pointer and
your integration will not work properly.

Above examples show part of YAML configuration file. Each of those lines has
to be part of a proper channel definition.

## Parsed channel definition

Each parsed channel type defines its own parameter key for fetching data
from `parser`.

Value of that parameter depends on used `parser` type. `Simple` parser use
indexes as a key (i.e. number 0, 1, 23). `Json` parser use text keys.

Additionally most values have additional `multiplier` parameter which allows to
convert input value by multiplying it by provided `multiplier`.

All channels can have `name` parameter defined. In next versions it will be
used to define relations between channels and other elements in your application.

Below you can find specification for each parsed channel type.

### `ThermometerParsed`

Add channel with "thermometer" type.

Mandatory parameter: `temperature` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

### `ThermHygroMeterParsed`

Add channel with "thermometer + hygrometer" type.

Mandatory parameters: `temperature` - defines key/index by which data is fetched
from `parser` for temperature value, `humidity` - defines key/index by which
data is fetched from `parser` for humidity value;
Optional parameter: `multiplier_temp` - defines multiplier for temperatur value
(you can put any floating point number), `multiplier_humi` - defines multiplier
for humidity value.

### `ImpulseCounterParsed`

Add channel with "impulse counter" type. You can define in Supla Cloud what
function it should use (i.e. energy meter, water meter, etc.)

Mandatory parameter: `counter` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

### `BinaryParsed`

Add channel with "binary sensor" type (on/off, 0/1 values etc.)

Mandatory parameter: `state` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

Values equal to "1" (approx) are considered as on/1/true. All other values
are considered as off/0/false.

### `ElectricityMeterParsed`

Add 3-phase electricity meter channel.

All paramters are optional, however it is reasonable to provide at least one ;).

There are two "global" parameters, and few parameters associated with
specific phases.

Global parameters:
* `frequency` - defines mapping for fetching voltage frequency from parser
* `multiplier` - defines multiplier value for frequency (default unit is Hz, so
if your data source provide data in Hz, you can put `multiplier: 1` or remove
this parameter completely.

Phase specific parameters:

Phase parameters are defined under `phase_1`, `phase_2`, and `phase_3` keys as
an array (each item starts with `-`).
Each parameter allows to configure optional `multiplier`.
List of allowed parameters:
* `voltage` - voltage in V
* `current` - current in A
* `fwd_act_energy` - total forward active energy in kWh
* `rvr_act_energy` - total reverse active energy in kWh
* `fwd_react_energy` - total forward reactive energy in kVA
* `rvr_react_energy` - total reverse reactive energy in kVA
* `power_active` - forward active power in W
* `rvr_power_active` - reverse active power in W
* `power_reactive` - Reactive power in VAR
* `power_apparent` - Apparent power in VA
* `phase_angle` - Phase angle (angle between voltage and current)
* `power_factor` - Power factor

`ElectricityMeterParsed` in example config file above contain definition of
mapping that can be used with
[wmbusmters](https://github.com/weetmuts/wmbusmeters) integration with Tauron
AMIplus meter on two tariff billing. Supla doesn't provide tariffs, however
in this example phase_1 energy is used for tariff 1 data, and phase_2 energy is
used for tariff 2 data.
Additionally this AMIplus integration provides separate fields for active power
as "consumption" and "production". Those are mapped to Supla active power and
reverse active power fields and then shown as one positive or negative number
(depending on actual value).

Below example may be used for
[wmbusmters](https://github.com/weetmuts/wmbusmeters) integration for Tauron
AMIplus meter on standard single tariff:

      - type: ElectricityMeterParsed
        parser:
          type: Json
        source:
          type: File
          file: tauron.json
        phase_1:
          - voltage: voltage_at_phase_1_v
          - fwd_act_energy: total_energy_consumption_kwh
          - rvr_act_energy: total_energy_production_kwh
          - power_active: current_power_consumption_kw
            multiplier: 1000
          - rvr_power_active: current_power_production_kw
            multiplier: 1000
        phase_2:
          - voltage: voltage_at_phase_2_v
        phase_3:
          - voltage: voltage_at_phase_3_v

### `ActionTriggerParsed`

Creates instance of "action trigger" channel. There is only one mandatory
parameter `name` which is used in other channels to reference this channel.

Action trigger can be used only in `BinaryParsed` and `CmdRelay` channels.
Here is example configuration of action trigger for `BinaryParsed` channel:

    action_trigger:
      - use: myAt2001
      - on_state: [-1, 0]
      - on_state_change: [0, 1, 1]
      - on_value: [5, 2]
      - on_value_change: [6, 7, 3]

Above config will use `ActionTriggerParsed` channel with `name` "myAt2001".
It will publish action "0" when channel enters state "-1" (offline).
Action "1" will be published when channel state changes from "0" (off) to "1" (on).
Action "2" will be published when value from `parser` is "5".
Action "3" will be published when value from `parser` changes from "6" to "7".

Both `BinaryParsed` and `CmdRelay` have state which can be equal to "0" (off) or
"1" (on). Additionally `CmdRelay` can have state "-1" which is set when channel
is offline.
So `state` refers to channel state reported to Supla. On the other hand, `value` represents
value which is read from `parser`. I.e. we can take input from file, which can have
values from -1 to 10. Value "-1" for `CmdRelay` will be interpret as "offline" (when
`offline_on_invalid_state` is set). Then by default value "1" is interpreted as
"on" state and all other values are interpreted as "off" (this can be modified
by setting `state_on_values` array).

Action trigger can be configured when channel enters selected state or value, and
for specific transition between states or values.

Parameters `on_state` and `on_value` will configure action trigger run when channel
enters selected state or value. First number in array defines state/value and second
number defines action number to be send. So format is:

    on_state: [STATE, ACTION]
    on_value: [VALUE, ACTION]

Similarly, parameters `on_state_change` and `on_value_change` will configure action
trigger run when state or value changes between two selected values. Format is:

    on_state_change: [FROM_STATE, TO_STATE, ACTION]
    on_value_change: [FROM_VALUE, TO_VALUE, ACTION]

It is allowed to configure multiple "on_" conditions to generate the same action.

For action, please use only following numbers: 0, 1, 2, 3, 4, 5, 6, and
10, 11, 12, 13, 14, 15.

When aciton is used and device is registered in Cloud, there will be action
trigger channel with actions corresponding to buttons, as defined in:
[proto.h](https://github.com/SUPLA/supla-device/blob/660a79b66676c995730b5aa8452543440f519772/src/supla-common/proto.h#L2111)

I.e. action 3 corresponds with:

    #define SUPLA_ACTION_CAP_TOGGLE_x2 (1 << 3)

(last number in bracket is action number, please check above link to `proto.h`
 for more details). Currently in Supla only actions
for buttons are defined, so we reuse them here.

### `HumidityParsed`
Add channel with "humidity" type.

Mandatory parameter: `humidity` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

### `PressureParsed`
Add channel with "pressure" type.

Mandatory parameter: `pressure` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

### `WindParsed`
Add channel with "wind" type.

Mandatory parameter: `wind` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).

### `RainParsed`
Add channel with "rain" type.

Mandatory parameter: `rain` - defines key/index by which data is fetched
from `parser`.
Optional parameter: `multiplier` - defines multiplier for fetched value
(you can put any floating point number).


## Battery level information for Parsed channels

Each Parsed channel may have additional `battery_level` and `multiplier_battery_level` field.
Battery level information is added to channel state response (the (i) button in mobile apps).
Battery level has to be in 0 to 100 range, otherwise device wont' be reported as battery
powered. Multiplier parameter allows to do some simple conversion. I.e. if battery level
in source is in 0 to 1 range, then you can provide multiplier with value 100 to convert it to
0 to 100 range.

# Running supla-device as a service

Following example will use `systemctl` for running supla-device as a service.

First, prepare configuration file in `/etc/supla-device.yaml`.

Create directory for GUID and state files with proper access rights:

    sudo mkdir -p /var/lib/supla-device
    sudo chown supla_user_name /var/lib/supla-device

Prepare service file: `/etc/systemd/system/supla-device.service`:

    [Unit]
    Description=Supla Device
    After=network-online.target

    [Service]
    User=supla_user_name
    ExecStart=/home/supla/supla-device/extras/examples/linux/build/supla-device-linux -s

    [Install]
    WantedBy=multi-user.target

Please adjust `supla_user_name` and ExecStart path to your needs.

Then call:

    sudo systemctl enable supla-device.service
    sudo systemctl start supla-device.service

And check if it works:

    sudo systemctl status supla-device.service

Example output:

    ● supla-device.service - Supla  Device
      Loaded: loaded (/etc/systemd/system/supla-device.service; enabled; vendor preset: enabled)
      Active: active (running) since Wed 2022-05-18 14:38:52 CEST; 8s ago
    Main PID: 7944 (supla-device-li)
       Tasks: 3 (limit: 4699)
      Memory: 980.0K
     CGroup: /system.slice/supla-device.service
             └─7944 /home/supla/supla-device/extras/examples/linux/build/supla-device-linux -s

    maj 18 14:38:52 supla-dev-01 supla-device-linux[7944]: Enter normal mode
    maj 18 14:38:52 supla-dev-01 supla-device-linux[7944]: Using Supla protocol version 16
    maj 18 14:38:52 supla-dev-01 supla-device-linux[7944]: LAST STATE ADDED: SuplaDevice initialized
    maj 18 14:38:52 supla-dev-01 supla-device-linux[7944]: Current status: [5] SuplaDevice initialized
    maj 18 14:38:52 supla-dev-01 supla-device-linux[7944]: Establishing connection with: beta-cloud.supla.org (port: 2016)
    maj 18 14:38:54 supla-dev-01 supla-device-linux[7944]: Connected to Supla Server
    maj 18 14:38:54 supla-dev-01 supla-device-linux[7944]: LAST STATE ADDED: Register in progress
    maj 18 14:38:54 supla-dev-01 supla-device-linux[7944]: Current status: [10] Register in progress
    maj 18 14:38:54 supla-dev-01 supla-device-linux[7944]: LAST STATE ADDED: Registered and ready
    maj 18 14:38:54 supla-dev-01 supla-device-linux[7944]: Current status: [17] Registered and ready

