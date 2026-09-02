# Home Assistant Discovery

Discovery traffic is generated separately from the public MQTT API.

## Relay

Channel type: `SUPLA_CHANNELTYPE_RELAY`

Channel functions: `SUPLA_CHANNELFNC_CONTROLLINGTHEGATE`, `SUPLA_CHANNELFNC_POWERSWITCH`

### Published topics

#### Cover discovery configuration

- Topic: `homeassistant/cover/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Cover functions and impulse relay functions.

Home Assistant MQTT discovery configuration for a cover or impulse relay channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 4:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

#### Light discovery configuration

- Topic: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Light-switch relay functions.

Home Assistant MQTT discovery configuration for a light relay channel.

Example:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

#### Switch discovery configuration

- Topic: `homeassistant/switch/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Switch-like relay functions.

Home Assistant MQTT discovery configuration for a switch relay channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Roller shutter

Channel type: `SUPLA_CHANNELTYPE_RELAY`

Channel function: `SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER`

### Published topics

#### Cover discovery configuration

- Topic: `homeassistant/cover/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Cover functions and impulse relay functions.

Home Assistant MQTT discovery configuration for a cover or impulse relay channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 4:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 5:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 6:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 7:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 8:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Dimmer

Channel type: `SUPLA_CHANNELTYPE_DIMMER`

Channel function: `SUPLA_CHANNELFNC_DIMMER`

### Published topics

#### Light discovery configuration

- Topic: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Light-switch relay functions.

Home Assistant MQTT discovery configuration for a light relay channel.

Example:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/4/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## RGB controller

Channel type: `SUPLA_CHANNELTYPE_RGBLEDCONTROLLER`

Channel function: `SUPLA_CHANNELFNC_RGBLIGHTING`

### Published topics

#### Light discovery configuration

- Topic: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Light-switch relay functions.

Home Assistant MQTT discovery configuration for a light relay channel.

Example:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/5/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Dimmer and RGB controller

Channel type: `SUPLA_CHANNELTYPE_DIMMERANDRGBLED`

Channel function: `SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING`

### Published topics

#### Light discovery configuration

- Topic: `homeassistant/light/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Light-switch relay functions.

Home Assistant MQTT discovery configuration for a light relay channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/6/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/6/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Thermometer

Channel type: `SUPLA_CHANNELTYPE_THERMOMETER`

Channel function: `0`

### Published topics

#### Sensor discovery configuration

- Topic: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Temperature, humidity, and electricity meter sensor channels.

Home Assistant MQTT discovery configuration for a temperature, humidity, or electricity meter sensor.

Example:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/2/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/temperature"
}
```

## Humidity and temperature sensor

Channel type: `SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR`

Channel function: `0`

### Published topics

#### Sensor discovery configuration

- Topic: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Temperature, humidity, and electricity meter sensor channels.

Home Assistant MQTT discovery configuration for a temperature, humidity, or electricity meter sensor.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/3/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/humidity"
}
```

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/3/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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
  "qos": 0,
  "ret": false,
  "opt": false,
  "stat_t": "~/state/temperature"
}
```

## HVAC

Channel type: `SUPLA_CHANNELTYPE_HVAC`

Channel functions: `SUPLA_CHANNELFNC_HVAC_THERMOSTAT`, `SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL`

### Published topics

#### HVAC discovery configuration

- Topic: `homeassistant/climate/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: HVAC thermostat channels.

Home Assistant MQTT discovery configuration for an HVAC channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Electricity meter

Channel type: `SUPLA_CHANNELTYPE_ELECTRICITY_METER`

Channel function: `SUPLA_CHANNELFNC_ELECTRICITY_METER`

### Published topics

#### Sensor discovery configuration

- Topic: `homeassistant/sensor/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Temperature, humidity, and electricity meter sensor channels.

Home Assistant MQTT discovery configuration for a temperature, humidity, or electricity meter sensor.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 4:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 5:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 6:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 7:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 8:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 9:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 10:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 11:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 12:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 13:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 14:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 15:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 16:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 17:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 18:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 19:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 20:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 21:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 22:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 23:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 24:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 25:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 26:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 27:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 28:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 29:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 30:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 31:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 32:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 33:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 34:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 35:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 36:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 37:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 38:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 39:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 40:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Binary sensor

Channel type: `SUPLA_CHANNELTYPE_BINARYSENSOR`

Channel function: `SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR`

### Published topics

#### Binary sensor discovery configuration

- Topic: `homeassistant/binary_sensor/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Binary sensor channels with a configured function.

Home Assistant MQTT discovery configuration for a binary sensor channel.

Example 1:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 2:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 3:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 4:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 5:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 6:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 7:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 8:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 9:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 10:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 11:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 12:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 13:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 14:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

Example 15:

```json
{
  "avty": [
    {
      "t": "prefix/supla/devices/my-device-0405ab/state/connected",
      "pl_avail": "true",
      "pl_not_avail": "false"
    },
    {
      "t": "prefix/supla/devices/my-device-0405ab/channels/0/state/available",
      "pl_avail": "true",
      "pl_not_avail": "false"
    }
  ],
  "avty_mode": "all",
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

## Channel

Channel type: `SUPLA_CHANNELTYPE_ACTIONTRIGGER`

Channel function: `SUPLA_CHANNELFNC_ACTIONTRIGGER`

### Published topics

#### Action trigger discovery configuration

- Topic: `homeassistant/device_automation/supla/000000000000_{channel}_{sub_id}/config`
- Payload type: `json`
- QoS: `0`
- Retain: `true`
- Availability: Action trigger channels.

Home Assistant MQTT discovery configuration for an action trigger capability.

Example 1:

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

Example 2:

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

Example 3:

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

Example 4:

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

Example 5:

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

Example 6:

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

Example 7:

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

Example 8:

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
