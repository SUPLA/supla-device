# Class size budgets

`extras/tools/check_class_sizes.py` compiles a generated probe with the AVR
Mega, ESP8266, ESP32, or host compiler. The probe contains `static_assert`
checks for the classes listed in `extras/tools/class_size_budgets.json`.

Run the fast host check with:

```sh
python3 extras/tools/check_class_sizes.py --platform host
```

Run the target checks with the installed Arduino cores:

```sh
python3 extras/tools/check_class_sizes.py \
  --platform avr --platform esp8266 --platform esp32
```

To refresh the exact current values in the JSON file, first measure each
target:

```sh
python3 extras/tools/check_class_sizes.py --measure \
  --platform host --platform avr --platform esp8266 --platform esp32
```

The checked-in limits currently equal those measured values; there is no
unreviewed safety margin.

The baseline was measured on 2026-09-21 with host `g++`,
`arduino:avr:mega`, `esp8266:esp8266:generic`, and
`esp32:esp32:esp32wrover` using the locally installed Arduino core versions.
The host probe uses the same `SUPLA_TEST` and `SUPLA_DEVICE` configuration as
the host library build.

The sensor entries cover the shared channel bases and the main public sensor
families: general-purpose measurements/meters, temperature/humidity/pressure,
electricity, binary and impulse sensors, pressure/wind/rain, distance, weight,
particle, container, temperature-drop, virtual, and multi-DS sensors.
Individual third-party driver wrappers are intentionally not all listed
separately; their common sensor base is guarded here.

All generated sketches and target build directories are placed below
`/tmp/codex` and removed when the check finishes. A budget is an upper bound
for the complete `sizeof(T)` on that architecture; it includes padding and
virtual bases, not just the fields added by a change.

This guard measures object/RAM layout only. It does not claim a limit for
flash, IRAM, stack, heap, or the total firmware image. Those values depend on
the selected application and linker configuration and must be checked by the
corresponding target build.

When a deliberate change needs more room, measure and review the target build
first, then update only the affected architecture entries in the JSON file.
Adding a new long-lived core class should include a corresponding entry.
