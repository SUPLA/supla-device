# SupLAN PoC2 ESP-IDF example

This development example runs the ordinary `SuplaDevice.begin()` and
`SuplaDevice.iterate()` lifecycle with the normal ESP-IDF Wi-Fi interface,
SUPLA Server SRPC, and the static PoC1 SupLAN fixture. It does not start a
second Wi-Fi lifecycle or call `Runtime::iterate()` directly.

The default fixture role is B (Device 1002). Select Device A in `menuconfig`
under **SupLAN PoC2 fixture** when required. Configure Wi-Fi and normal SUPLA
Server credentials through the regular sd4linux/ESP device configuration
flow. The SRPC GUID/AuthKey and the deterministic SupLAN fixture keys are
separate.

Build and flash with ESP-IDF 6.x:

```sh
export SUPLA_DEVICE_PATH=/path/to/supla-device
idf.py -C extras/examples/suplan_poc2_esp_idf build
idf.py -C extras/examples/suplan_poc2_esp_idf -p /dev/ttyUSB0 -b 115200 app-flash monitor
```

The serial console supports `show-status`, `show-resources`, `show-counters`,
`show-pools`, `read`, `control`, `set-resource-value`, `emit-action`,
`forget-session`, and `clear-endpoint`. These commands are development hooks;
the relay change still uses the regular local channel and its update path.

SupLAN UDP opens after the normal network becomes ready and closes through the
standard ProtocolLayer disconnect path when that network is lost. The normal
SRPC registration is not required for local SupLAN operation. The fixture
identities and keys are test-only credentials.
