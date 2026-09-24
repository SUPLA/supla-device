# SupLAN PoC1 ESP-IDF harness

This standalone ESP-IDF project builds the same portable core and static test
profile used by the Linux harness. Build it twice, once for role A and once for
role B, and flash two ESP32 devices on the same IPv4 multicast-enabled LAN.

Set `SUPLA_DEVICE_PATH` to the `supla-device` worktree, run `idf.py menuconfig`,
enter the test AP credentials, and select the node role under **SupLAN PoC1**.
The deterministic key material is a test fixture only and must not be used in
deployed products. Both images listen on UDP port 2016 and use
`239.255.201.6:2016` for authenticated LOCATE.

After flashing, use the serial console at the configured IDF baud rate. The
harness exposes the same PoC commands as the Linux program for READ, CONTROL,
Action Trigger, state notification, retry/fault injection, fragmentation,
bounded flood diagnostics, and pool/counter reports. Hardware results must be
recorded separately from host tests; this project does not emulate ESP32.
