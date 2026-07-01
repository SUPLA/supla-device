# ESP-IDF Example

This example can expose a development-only TCP log stream when the SUPLA
insecure debug interface is enabled.

## TCP Log Stream

Enable the option in menuconfig:

```sh
idf.py menuconfig
```

Then select:

```text
Component config -> supla-device -> Supla debug build
Component config -> supla-device -> Enable insecure SUPLA debug interfaces
```

Build and flash the example:

```sh
idf.py build flash monitor
```

The example starts `Supla::Debug::DebugLogTcpServer` on TCP port `7778` when
`SUPLA_INSECURE_DEBUG_INTERFACE` is enabled. Connect to the device IP from the
same network:

```sh
nc <device-ip> 7778
```

Only log lines emitted after the TCP client connects are streamed. This is an
insecure development interface. Do not enable it in production firmware and do
not expose it to untrusted networks.
