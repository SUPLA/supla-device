# sd4linux Insecure Debug Socket

This is a local debug/test interface for the sd4linux example. It listens on a
Unix domain socket, reads one JSON command per line, and executes it inside the
running process.

The interface is enabled only in builds compiled with
`SUPLA_INSECURE_DEBUG_INTERFACE=1`. Treat it as a development hook, not as a
stable remote API. Do not expose the socket to untrusted users.

## Build

From the repository root:

```sh
cmake -S extras/examples/linux -B extras/examples/linux/build
cmake --build extras/examples/linux/build -j2
```

## Start With Debug Socket

Run sd4linux with a Unix socket path. The process creates the socket and removes
stale socket files from previous runs.

```sh
extras/examples/linux/build/supla-device-linux \
  --config extras/examples/linux/supla-device.yaml \
  --debug-socket /tmp/sd4linux-debug.sock \
  --debug
```

Commands can then be sent from another shell:

```sh
printf '%s\n' '{"calcfg":"getInstanceList"}' | nc -U /tmp/sd4linux-debug.sock
```

Results are returned as JSON lines on the same socket. CALCFG commands are
injected locally with `SuplaDevice.handleCalcfgFromServer()` and do not send a
reply to the SUPLA server.

## Stream SUPLA Logs Over TCP

The same insecure debug build can stream live SUPLA logs to a TCP client. This
is live-only: a client receives logs emitted after it connects, without backlog.

```sh
extras/examples/linux/build/supla-device-linux \
  --config extras/examples/linux/supla-device.yaml \
  --debug-log-port 7778 \
  --debug
```

Connect from another shell:

```sh
nc 127.0.0.1 7778
```

Use `--debug-log-port 0` or omit the option to keep the TCP log stream disabled.

## Suplet CALCFG Examples

Save a downloaded virtual relay definition:

```sh
printf '%s\n' '{"calcfg":"saveDefinition","definitionId":2010,"definitionVersion":1,"definitionJson":"{\"schemaVersion\":1,\"handlerVersion\":1,\"definitionId\":2010,\"definitionVersion\":1,\"maxInstances\":3,\"category\":\"virtual\",\"kind\":\"virtualRelay\",\"parameters\":[{\"key\":\"relay.count\",\"type\":\"uint8\",\"default\":1,\"min\":1,\"max\":4,\"lifecycle\":\"createOnly\",\"affectsTopology\":true},{\"key\":\"mode\",\"type\":\"enum\",\"default\":\"avg\",\"values\":[\"avg\",\"min\",\"max\"],\"required\":true},{\"key\":\"host\",\"type\":\"string\",\"required\":true}],\"channels\":[{\"channelId\":1,\"key\":\"relay\",\"kind\":\"virtualRelay\",\"function\":\"powerSwitch\",\"caption\":\"Param relay\"}]}"}' | nc -U /tmp/sd4linux-debug.sock
```

Create an instance. `instanceId: 0` asks the runtime to allocate a free slot.

```sh
printf '%s\n' '{"calcfg":"upsertInstance","instanceId":0,"definitionId":2010,"definitionVersion":1,"paramsJson":"{\"relay.count\":2,\"mode\":\"max\",\"host\":\"192.168.1.50\"}","state":"active"}' | nc -U /tmp/sd4linux-debug.sock
```

List instances:

```sh
printf '%s\n' '{"calcfg":"getInstanceList"}' | nc -U /tmp/sd4linux-debug.sock
```

Inspect an instance. Replace `2` with the instance id returned by the list:

```sh
printf '%s\n' '{"calcfg":"getInstanceInfo","instanceId":2}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"calcfg":"getInstanceConfig","instanceId":2}' | nc -U /tmp/sd4linux-debug.sock
```

Save version 2 of the same definition:

```sh
printf '%s\n' '{"calcfg":"saveDefinition","definitionId":2010,"definitionVersion":2,"definitionJson":"{\"schemaVersion\":1,\"handlerVersion\":1,\"definitionId\":2010,\"definitionVersion\":2,\"maxInstances\":3,\"category\":\"virtual\",\"kind\":\"virtualRelay\",\"parameters\":[{\"key\":\"relay.count\",\"type\":\"uint8\",\"default\":2,\"min\":1,\"max\":4,\"lifecycle\":\"createOnly\",\"affectsTopology\":true},{\"key\":\"startup.delay\",\"type\":\"uint16\",\"default\":5,\"min\":0,\"max\":3600,\"lifecycle\":\"createOnly\"},{\"key\":\"mode\",\"type\":\"enum\",\"default\":\"avg\",\"values\":[\"avg\",\"min\",\"max\"],\"required\":true},{\"key\":\"host\",\"type\":\"string\",\"required\":true},{\"key\":\"display.name\",\"type\":\"string\",\"default\":\"Relay group\"}],\"channels\":[{\"channelId\":1,\"key\":\"relay\",\"kind\":\"virtualRelay\",\"function\":\"powerSwitch\",\"caption\":\"Param relay v2\"}]}"}' | nc -U /tmp/sd4linux-debug.sock
```

Upgrade an existing instance from definition version 1 to 2:

```sh
printf '%s\n' '{"calcfg":"upgradeInstance","instanceId":1,"definitionId":2010,"fromDefinitionVersion":1,"toDefinitionVersion":2,"paramsJson":"{\"relay.count\":2,\"startup.delay\":5,\"mode\":\"max\",\"host\":\"192.168.1.50\",\"display.name\":\"Relay group v2\"}"}' | nc -U /tmp/sd4linux-debug.sock
```

Inspect the upgraded instance:

```sh
printf '%s\n' '{"calcfg":"getInstanceInfo","instanceId":1}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"calcfg":"getInstanceConfig","instanceId":1}' | nc -U /tmp/sd4linux-debug.sock
```

Remove an unused downloaded definition:

```sh
printf '%s\n' '{"calcfg":"removeDefinition","definitionId":2010,"definitionVersion":1}' | nc -U /tmp/sd4linux-debug.sock
```

Other currently supported CALCFG operations:

```sh
printf '%s\n' '{"calcfg":"getCapabilities"}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"calcfg":"getDefinitionList"}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"calcfg":"getDefinitionConfig","definitionId":2010,"definitionVersion":1}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"calcfg":"getInstanceCount"}' | nc -U /tmp/sd4linux-debug.sock
```

## Direct Suplet Command JSON

The socket also accepts the direct Suplet command JSON handled by
`SuplaDevice.validateSupletCommandJson()` and
`SuplaDevice.applySupletCommandJson()`. Use CALCFG examples above when testing
the protocol-shaped path. Use direct command JSON when testing the internal
server-config handler contract.
