# Home Assistant Discovery

Ruch Home Assistant Discovery jest generowany oddzielnie od publicznego API MQTT.

## Przekaźnik

Typ kanału: `SUPLA_CHANNELTYPE_RELAY`

Funkcje kanału: `SUPLA_CHANNELFNC_CONTROLLINGTHEGATE`, `SUPLA_CHANNELFNC_POWERSWITCH`

### Publikowane topiki

#### Konfiguracja discovery osłony

- Topik: `homeassistant/cover/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje osłon i przekaźników impulsowych.

Konfiguracja MQTT Discovery Home Assistanta dla kanału osłony lub przekaźnika impulsowego.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Door lock",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "payload_open": "true",
  "payload_close": null,
  "payload_stop": null,
  "dev_cla": "door"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Garage door",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "payload_open": "true",
  "payload_close": null,
  "payload_stop": null,
  "dev_cla": "garage"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Gate",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "payload_open": "true",
  "payload_close": null,
  "payload_stop": null,
  "dev_cla": "gate"
}
```

Przykład 4:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Gateway lock",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "payload_open": "true",
  "payload_close": null,
  "payload_stop": null,
  "dev_cla": "door"
}
```

#### Konfiguracja discovery światła

- Topik: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje przekaźników światła.

Konfiguracja MQTT Discovery Home Assistanta dla kanału przekaźnika światła.

Przykład:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Light switch",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "pl_on": "true",
  "pl_off": "false"
}
```

#### Konfiguracja discovery przełącznika

- Topik: `homeassistant/switch/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje przekaźników przełączających.

Konfiguracja MQTT Discovery Home Assistanta dla kanału przekaźnika przełączającego.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Heater/cooling switch",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "pl_on": "true",
  "pl_off": "false"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Power switch",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "pl_on": "true",
  "pl_off": "false"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Pump switch",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/set/on",
  "pl_on": "true",
  "pl_off": "false"
}
```

## Roleta

Typ kanału: `SUPLA_CHANNELTYPE_RELAY`

Funkcja kanału: `SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER`

### Publikowane topiki

#### Konfiguracja discovery osłony

- Topik: `homeassistant/cover/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje osłon i przekaźników impulsowych.

Konfiguracja MQTT Discovery Home Assistanta dla kanału osłony lub przekaźnika impulsowego.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Curtain",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "curtain"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Facade blind",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "tilt_cmd_t": "~/set/tilt",
  "tilt_status_t": "~/state/tilt",
  "tilt_min": 100,
  "tilt_max": 0,
  "tilt_opened_value": 0,
  "tilt_closed_value": 100,
  "tilt_status_tpl": "{% if int(value, default=0) <= 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}",
  "dev_cla": "shutter"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Projector screen",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "shade"
}
```

Przykład 4:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Roller garage door",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "garage"
}
```

Przykład 5:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Roller shutter",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "shutter"
}
```

Przykład 6:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Roof window",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "window"
}
```

Przykład 7:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Terrace awning",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "dev_cla": "awning"
}
```

Przykład 8:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Vertical blind",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "cmd_t": "~/execute_action",
  "pl_open": "REVEAL",
  "pl_cls": "SHUT",
  "pl_stop": "STOP",
  "set_pos_t": "~/set/closing_percentage",
  "pos_t": "~/state/shut",
  "pos_open": 0,
  "pos_clsd": 100,
  "pos_tpl": "{% if value is defined %}{% if value | int < 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}{% else %}0{% endif %}",
  "tilt_cmd_t": "~/set/tilt",
  "tilt_status_t": "~/state/tilt",
  "tilt_min": 100,
  "tilt_max": 0,
  "tilt_opened_value": 0,
  "tilt_closed_value": 100,
  "tilt_status_tpl": "{% if int(value, default=0) <= 0 %}0{% elif value | int > 100 %}100{% else %}{{value | int}}{% endif %}",
  "dev_cla": "shutter"
}
```

## Ściemniacz

Typ kanału: `SUPLA_CHANNELTYPE_DIMMER`

Funkcja kanału: `SUPLA_CHANNELFNC_DIMMER`

### Publikowane topiki

#### Konfiguracja discovery światła

- Topik: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje przekaźników światła.

Konfiguracja MQTT Discovery Home Assistanta dla kanału przekaźnika światła.

Przykład:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/4",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "Dimmer",
  "uniq_id": "supla_000000000000_4_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/execute_action",
  "pl_on": "TURN_ON",
  "pl_off": "TURN_OFF",
  "stat_val_tpl": "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}",
  "on_cmd_type": "last",
  "bri_cmd_t": "~/set/brightness",
  "bri_scl": 100,
  "bri_stat_t": "~/state/brightness"
}
```

## Sterownik RGB

Typ kanału: `SUPLA_CHANNELTYPE_RGBLEDCONTROLLER`

Funkcja kanału: `SUPLA_CHANNELFNC_RGBLIGHTING`

### Publikowane topiki

#### Konfiguracja discovery światła

- Topik: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje przekaźników światła.

Konfiguracja MQTT Discovery Home Assistanta dla kanału przekaźnika światła.

Przykład:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/5",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "RGB Lighting",
  "uniq_id": "supla_000000000000_5_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/on",
  "cmd_t": "~/execute_action",
  "pl_on": "TURN_ON",
  "pl_off": "TURN_OFF",
  "stat_val_tpl": "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}",
  "on_cmd_type": "last",
  "bri_cmd_t": "~/set/color_brightness",
  "bri_scl": 100,
  "bri_stat_t": "~/state/color_brightness",
  "rgb_stat_t": "~/state/color",
  "rgb_cmd_t": "~/set/color"
}
```

## Sterownik ściemniacza i RGB

Typ kanału: `SUPLA_CHANNELTYPE_DIMMERANDRGBLED`

Funkcja kanału: `SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING`

### Publikowane topiki

#### Konfiguracja discovery światła

- Topik: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje przekaźników światła.

Konfiguracja MQTT Discovery Home Assistanta dla kanału przekaźnika światła.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/6",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "Dimmer",
  "uniq_id": "supla_000000000000_6_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/dimmer/on",
  "cmd_t": "~/execute_action/dimmer",
  "pl_on": "TURN_ON",
  "pl_off": "TURN_OFF",
  "stat_val_tpl": "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}",
  "on_cmd_type": "last",
  "bri_cmd_t": "~/set/brightness",
  "bri_scl": 100,
  "bri_stat_t": "~/state/brightness"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/6",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "RGB Lighting",
  "uniq_id": "supla_000000000000_6_1",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/rgb/on",
  "cmd_t": "~/execute_action/rgb",
  "pl_on": "TURN_ON",
  "pl_off": "TURN_OFF",
  "stat_val_tpl": "{% if value == \"true\" %}TURN_ON{% else %}TURN_OFF{% endif %}",
  "on_cmd_type": "last",
  "bri_cmd_t": "~/set/color_brightness",
  "bri_scl": 100,
  "bri_stat_t": "~/state/color_brightness",
  "rgb_stat_t": "~/state/color",
  "rgb_cmd_t": "~/set/color"
}
```

## Termometr

Typ kanału: `SUPLA_CHANNELTYPE_THERMOMETER`

Funkcja kanału: `0`

### Publikowane topiki

#### Konfiguracja discovery czujnika

- Topik: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały czujników temperatury, wilgotności i liczników energii.

Konfiguracja MQTT Discovery Home Assistanta dla czujnika temperatury, wilgotności lub licznika energii.

Przykład:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/2",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#2 Temperature",
  "uniq_id": "supla_000000000000_2_0",
  "dev_cla": "temperature",
  "unit_of_meas": "°C",
  "stat_cla": "measurement",
  "expire_after": 30,
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/temperature"
}
```

## Czujnik wilgotności i temperatury

Typ kanału: `SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR`

Funkcja kanału: `0`

### Publikowane topiki

#### Konfiguracja discovery czujnika

- Topik: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały czujników temperatury, wilgotności i liczników energii.

Konfiguracja MQTT Discovery Home Assistanta dla czujnika temperatury, wilgotności lub licznika energii.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/3",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#3 Humidity",
  "dev_cla": "humidity",
  "stat_cla": "measurement",
  "unit_of_meas": "%",
  "uniq_id": "supla_000000000000_3_0",
  "expire_after": 30,
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/humidity"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/3",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#3 Temperature",
  "uniq_id": "supla_000000000000_3_1",
  "dev_cla": "temperature",
  "unit_of_meas": "°C",
  "stat_cla": "measurement",
  "expire_after": 30,
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/temperature"
}
```

## HVAC

Typ kanału: `SUPLA_CHANNELTYPE_HVAC`

Funkcje kanału: `SUPLA_CHANNELFNC_HVAC_THERMOSTAT`, `SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL`

### Publikowane topiki

#### Konfiguracja discovery HVAC

- Topik: `homeassistant/climate/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały termostatów HVAC.

Konfiguracja MQTT Discovery Home Assistanta dla kanału HVAC.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Thermostat",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "action_topic": "~/state/action",
  "current_temperature_topic": "prefix/supla/devices/my-device-0405ab/channels/-1/state/temperature",
  "current_humidity_topic": "None",
  "max_temp": "-327.68",
  "min_temp": "-327.68",
  "modes": [
    "off",
    "auto",
    "cool"
  ],
  "mode_stat_t": "~/state/mode",
  "mode_command_topic": "~/execute_action",
  "power_command_topic": "~/execute_action",
  "payload_off": "turn_off",
  "payload_on": "turn_on",
  "temperature_unit": "C",
  "temp_step": "0.1",
  "temperature_command_topic": "~/set/temperature_setpoint",
  "temperature_state_topic": "~/state/temperature_setpoint"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Thermostat",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "action_topic": "~/state/action",
  "current_temperature_topic": "prefix/supla/devices/my-device-0405ab/channels/-1/state/temperature",
  "current_humidity_topic": "None",
  "max_temp": "-327.68",
  "min_temp": "-327.68",
  "modes": [
    "off",
    "auto",
    "heat",
    "cool",
    "heat_cool"
  ],
  "mode_stat_t": "~/state/mode",
  "mode_command_topic": "~/execute_action",
  "power_command_topic": "~/execute_action",
  "payload_off": "turn_off",
  "payload_on": "turn_on",
  "temperature_unit": "C",
  "temp_step": "0.1",
  "temperature_high_command_topic": "~/set/temperature_setpoint_cool/",
  "temperature_high_state_topic": "~/state/temperature_setpoint_cool/",
  "temperature_low_command_topic": "~/set/temperature_setpoint_heat/",
  "temperature_low_state_topic": "~/state/temperature_setpoint_heat/"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Thermostat",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "action_topic": "~/state/action",
  "current_temperature_topic": "prefix/supla/devices/my-device-0405ab/channels/-1/state/temperature",
  "current_humidity_topic": "None",
  "max_temp": "-327.68",
  "min_temp": "-327.68",
  "modes": [
    "off",
    "auto",
    "heat"
  ],
  "mode_stat_t": "~/state/mode",
  "mode_command_topic": "~/execute_action",
  "power_command_topic": "~/execute_action",
  "payload_off": "turn_off",
  "payload_on": "turn_on",
  "temperature_unit": "C",
  "temp_step": "0.1",
  "temperature_command_topic": "~/set/temperature_setpoint",
  "temperature_state_topic": "~/state/temperature_setpoint"
}
```

## Licznik energii elektrycznej

Typ kanału: `SUPLA_CHANNELTYPE_ELECTRICITY_METER`

Funkcja kanału: `SUPLA_CHANNELFNC_ELECTRICITY_METER`

### Publikowane topiki

#### Konfiguracja discovery czujnika

- Topik: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały czujników temperatury, wilgotności i liczników energii.

Konfiguracja MQTT Discovery Home Assistanta dla czujnika temperatury, wilgotności lub licznika energii.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward active energy)",
  "uniq_id": "supla_000000000000_0_1",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/total_forward_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse active energy)",
  "uniq_id": "supla_000000000000_0_2",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/total_reverse_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward balanced active energy)",
  "uniq_id": "supla_000000000000_0_3",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/total_forward_balanced_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 4:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse balanced active energy)",
  "uniq_id": "supla_000000000000_0_4",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/total_reverse_balanced_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 5:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward active energy - Phase 1)",
  "uniq_id": "supla_000000000000_0_5",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/1/total_forward_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 6:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse active energy - Phase 1)",
  "uniq_id": "supla_000000000000_0_6",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/1/total_reverse_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 7:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward reactive energy - Phase 1)",
  "uniq_id": "supla_000000000000_0_7",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/1/total_forward_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 8:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse reactive energy - Phase 1)",
  "uniq_id": "supla_000000000000_0_8",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/1/total_reverse_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 9:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Frequency - Phase 1)",
  "uniq_id": "supla_000000000000_0_9",
  "qos": 0,
  "unit_of_meas": "Hz",
  "stat_t": "~/state/phases/1/frequency",
  "stat_cla": "measurement",
  "dev_cla": "frequency"
}
```

Przykład 10:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Voltage - Phase 1)",
  "uniq_id": "supla_000000000000_0_10",
  "qos": 0,
  "unit_of_meas": "V",
  "stat_t": "~/state/phases/1/voltage",
  "stat_cla": "measurement",
  "dev_cla": "voltage"
}
```

Przykład 11:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Current - Phase 1)",
  "uniq_id": "supla_000000000000_0_11",
  "qos": 0,
  "unit_of_meas": "A",
  "stat_t": "~/state/phases/1/current",
  "stat_cla": "measurement",
  "dev_cla": "current"
}
```

Przykład 12:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power active - Phase 1)",
  "uniq_id": "supla_000000000000_0_12",
  "qos": 0,
  "unit_of_meas": "W",
  "stat_t": "~/state/phases/1/power_active",
  "stat_cla": "measurement",
  "dev_cla": "power"
}
```

Przykład 13:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power reactive - Phase 1)",
  "uniq_id": "supla_000000000000_0_13",
  "qos": 0,
  "unit_of_meas": "var",
  "stat_t": "~/state/phases/1/power_reactive",
  "stat_cla": "measurement",
  "dev_cla": "reactive_power"
}
```

Przykład 14:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power apparent - Phase 1)",
  "uniq_id": "supla_000000000000_0_14",
  "qos": 0,
  "unit_of_meas": "VA",
  "stat_t": "~/state/phases/1/power_apparent",
  "stat_cla": "measurement",
  "dev_cla": "apparent_power"
}
```

Przykład 15:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power factor - Phase 1)",
  "uniq_id": "supla_000000000000_0_15",
  "qos": 0,
  "unit_of_meas": "",
  "stat_t": "~/state/phases/1/power_factor",
  "stat_cla": "measurement",
  "dev_cla": "power_factor"
}
```

Przykład 16:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Phase angle - Phase 1)",
  "uniq_id": "supla_000000000000_0_16",
  "qos": 0,
  "unit_of_meas": "°",
  "stat_t": "~/state/phases/1/phase_angle",
  "stat_cla": "measurement"
}
```

Przykład 17:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward active energy - Phase 2)",
  "uniq_id": "supla_000000000000_0_17",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/2/total_forward_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 18:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse active energy - Phase 2)",
  "uniq_id": "supla_000000000000_0_18",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/2/total_reverse_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 19:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward reactive energy - Phase 2)",
  "uniq_id": "supla_000000000000_0_19",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/2/total_forward_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 20:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse reactive energy - Phase 2)",
  "uniq_id": "supla_000000000000_0_20",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/2/total_reverse_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 21:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Frequency - Phase 2)",
  "uniq_id": "supla_000000000000_0_21",
  "qos": 0,
  "unit_of_meas": "Hz",
  "stat_t": "~/state/phases/2/frequency",
  "stat_cla": "measurement",
  "dev_cla": "frequency"
}
```

Przykład 22:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Voltage - Phase 2)",
  "uniq_id": "supla_000000000000_0_22",
  "qos": 0,
  "unit_of_meas": "V",
  "stat_t": "~/state/phases/2/voltage",
  "stat_cla": "measurement",
  "dev_cla": "voltage"
}
```

Przykład 23:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Current - Phase 2)",
  "uniq_id": "supla_000000000000_0_23",
  "qos": 0,
  "unit_of_meas": "A",
  "stat_t": "~/state/phases/2/current",
  "stat_cla": "measurement",
  "dev_cla": "current"
}
```

Przykład 24:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power active - Phase 2)",
  "uniq_id": "supla_000000000000_0_24",
  "qos": 0,
  "unit_of_meas": "W",
  "stat_t": "~/state/phases/2/power_active",
  "stat_cla": "measurement",
  "dev_cla": "power"
}
```

Przykład 25:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power reactive - Phase 2)",
  "uniq_id": "supla_000000000000_0_25",
  "qos": 0,
  "unit_of_meas": "var",
  "stat_t": "~/state/phases/2/power_reactive",
  "stat_cla": "measurement",
  "dev_cla": "reactive_power"
}
```

Przykład 26:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power apparent - Phase 2)",
  "uniq_id": "supla_000000000000_0_26",
  "qos": 0,
  "unit_of_meas": "VA",
  "stat_t": "~/state/phases/2/power_apparent",
  "stat_cla": "measurement",
  "dev_cla": "apparent_power"
}
```

Przykład 27:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power factor - Phase 2)",
  "uniq_id": "supla_000000000000_0_27",
  "qos": 0,
  "unit_of_meas": "",
  "stat_t": "~/state/phases/2/power_factor",
  "stat_cla": "measurement",
  "dev_cla": "power_factor"
}
```

Przykład 28:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Phase angle - Phase 2)",
  "uniq_id": "supla_000000000000_0_28",
  "qos": 0,
  "unit_of_meas": "°",
  "stat_t": "~/state/phases/2/phase_angle",
  "stat_cla": "measurement"
}
```

Przykład 29:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward active energy - Phase 3)",
  "uniq_id": "supla_000000000000_0_29",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/3/total_forward_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 30:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse active energy - Phase 3)",
  "uniq_id": "supla_000000000000_0_30",
  "qos": 0,
  "unit_of_meas": "kWh",
  "stat_t": "~/state/phases/3/total_reverse_active_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "energy"
}
```

Przykład 31:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total forward reactive energy - Phase 3)",
  "uniq_id": "supla_000000000000_0_31",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/3/total_forward_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 32:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Total reverse reactive energy - Phase 3)",
  "uniq_id": "supla_000000000000_0_32",
  "qos": 0,
  "unit_of_meas": "kvarh",
  "stat_t": "~/state/phases/3/total_reverse_reactive_energy",
  "stat_cla": "total_increasing",
  "dev_cla": "reactive_energy"
}
```

Przykład 33:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Frequency - Phase 3)",
  "uniq_id": "supla_000000000000_0_33",
  "qos": 0,
  "unit_of_meas": "Hz",
  "stat_t": "~/state/phases/3/frequency",
  "stat_cla": "measurement",
  "dev_cla": "frequency"
}
```

Przykład 34:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Voltage - Phase 3)",
  "uniq_id": "supla_000000000000_0_34",
  "qos": 0,
  "unit_of_meas": "V",
  "stat_t": "~/state/phases/3/voltage",
  "stat_cla": "measurement",
  "dev_cla": "voltage"
}
```

Przykład 35:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Current - Phase 3)",
  "uniq_id": "supla_000000000000_0_35",
  "qos": 0,
  "unit_of_meas": "A",
  "stat_t": "~/state/phases/3/current",
  "stat_cla": "measurement",
  "dev_cla": "current"
}
```

Przykład 36:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power active - Phase 3)",
  "uniq_id": "supla_000000000000_0_36",
  "qos": 0,
  "unit_of_meas": "W",
  "stat_t": "~/state/phases/3/power_active",
  "stat_cla": "measurement",
  "dev_cla": "power"
}
```

Przykład 37:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power reactive - Phase 3)",
  "uniq_id": "supla_000000000000_0_37",
  "qos": 0,
  "unit_of_meas": "var",
  "stat_t": "~/state/phases/3/power_reactive",
  "stat_cla": "measurement",
  "dev_cla": "reactive_power"
}
```

Przykład 38:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power apparent - Phase 3)",
  "uniq_id": "supla_000000000000_0_38",
  "qos": 0,
  "unit_of_meas": "VA",
  "stat_t": "~/state/phases/3/power_apparent",
  "stat_cla": "measurement",
  "dev_cla": "apparent_power"
}
```

Przykład 39:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Power factor - Phase 3)",
  "uniq_id": "supla_000000000000_0_39",
  "qos": 0,
  "unit_of_meas": "",
  "stat_t": "~/state/phases/3/power_factor",
  "stat_cla": "measurement",
  "dev_cla": "power_factor"
}
```

Przykład 40:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Electricity Meter (Phase angle - Phase 3)",
  "uniq_id": "supla_000000000000_0_40",
  "qos": 0,
  "unit_of_meas": "°",
  "stat_t": "~/state/phases/3/phase_angle",
  "stat_cla": "measurement"
}
```

## Czujnik binarny

Typ kanału: `SUPLA_CHANNELTYPE_BINARYSENSOR`

Funkcja kanału: `SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR`

### Publikowane topiki

#### Konfiguracja discovery czujnika binarnego

- Topik: `homeassistant/binary_sensor/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały czujników binarnych ze skonfigurowaną funkcją.

Konfiguracja MQTT Discovery Home Assistanta dla kanału czujnika binarnego.

Przykład 1:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Alarm armament sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 2:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Binary sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 3:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Container level sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 4:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Door sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "door",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 5:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Flood sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "moisture"
}
```

Przykład 6:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Garage door sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "garage_door",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 7:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Gate sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "garage_door",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 8:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Gateway sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "door",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 9:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Hotel card sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 10:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Liquid sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "moisture"
}
```

Przykład 11:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Mail sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 12:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Motion sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state"
}
```

Przykład 13:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Roller shutter sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "window",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 14:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Roof window sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "window",
  "payload_on": "open",
  "payload_off": "closed"
}
```

Przykład 15:

```json
{
  "avty_t": "prefix/supla/devices/my-device-0405ab/state/connected",
  "pl_avail": "true",
  "pl_not_avail": "false",
  "~": "prefix/supla/devices/my-device-0405ab/channels/0",
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "name": "#0 Window sensor",
  "uniq_id": "supla_000000000000_0_0",
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state",
  "dev_cla": "window",
  "payload_on": "open",
  "payload_off": "closed"
}
```

## Kanał

Typ kanału: `SUPLA_CHANNELTYPE_ACTIONTRIGGER`

Funkcja kanału: `SUPLA_CHANNELFNC_ACTIONTRIGGER`

### Publikowane topiki

#### Konfiguracja discovery wyzwalacza akcji

- Topik: `homeassistant/device_automation/supla/000000000000_{channel}_{sub_id}/config`
- Typ payloadu: `json`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały wyzwalaczy akcji.

Konfiguracja MQTT Discovery Home Assistanta dla możliwości wyzwalacza akcji.

Przykład 1:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_long_press",
  "type": "button_long_press",
  "subtype": "button_1",
  "payload": "button_long_press",
  "qos": 0,
  "ret": false
}
```

Przykład 2:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_short_press",
  "type": "button_short_press",
  "subtype": "button_1",
  "payload": "button_short_press",
  "qos": 0,
  "ret": false
}
```

Przykład 3:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_double_press",
  "type": "button_double_press",
  "subtype": "button_1",
  "payload": "button_double_press",
  "qos": 0,
  "ret": false
}
```

Przykład 4:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_triple_press",
  "type": "button_triple_press",
  "subtype": "button_1",
  "payload": "button_triple_press",
  "qos": 0,
  "ret": false
}
```

Przykład 5:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_quadruple_press",
  "type": "button_quadruple_press",
  "subtype": "button_1",
  "payload": "button_quadruple_press",
  "qos": 0,
  "ret": false
}
```

Przykład 6:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_quintuple_press",
  "type": "button_quintuple_press",
  "subtype": "button_1",
  "payload": "button_quintuple_press",
  "qos": 0,
  "ret": false
}
```

Przykład 7:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_turn_on",
  "type": "button_turn_on",
  "subtype": "button_1",
  "payload": "button_turn_on",
  "qos": 0,
  "ret": false
}
```

Przykład 8:

```json
{
  "dev": {
    "ids": "my-device-0405ab",
    "mf": "Unknown",
    "name": "My Device",
    "sw": ""
  },
  "automation_type": "trigger",
  "topic": "prefix/supla/devices/my-device-0405ab/channels/0/button_turn_off",
  "type": "button_turn_off",
  "subtype": "button_1",
  "payload": "button_turn_off",
  "qos": 0,
  "ret": false
}
```
