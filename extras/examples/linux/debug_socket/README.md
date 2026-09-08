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

## Channel Value Commands

The `channelValue` operation injects the same `TSD_SuplaChannelNewValue` that a
device normally receives from the server. It finds the element owning the
selected channel and calls its regular `handleNewValueFromServer()` method.

Switch Relay channel 0 to weekly schedule mode:

```sh
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":6}' | nc -U /tmp/sd4linux-debug.sock
```

Switch it back to manual mode, or send an ordinary forced/automatic command:

```sh
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":7}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":3}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":4}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":5}' | nc -U /tmp/sd4linux-debug.sock
```

Relay modes are: `0` not set, `1` on once, `2` off once, `3` forced on, `4`
forced off, `5` automatic, `6` switch to weekly schedule, and `7` switch to
manual. For a synthesized server value, modes `1` and `3` carry the ON state,
while modes `2` and `4` carry the OFF state. Such commands remain subject to an
active weekly schedule; switch to manual first if the current forced program
blocks the requested transition.

Switch Action Trigger channel 1 to weekly schedule, manual, locked, or unlocked:

```sh
printf '%s\n' '{"op":"channelValue","channelNumber":1,"buttonMode":4}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":1,"buttonMode":5}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":1,"buttonMode":1}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":1,"buttonMode":0}' | nc -U /tmp/sd4linux-debug.sock
```

Action Trigger modes are: `0` not set/unlocked, `1` locked, `4` switch to
weekly schedule, and `5` switch to manual.

For arbitrary channel commands, pass the complete 8-byte protocol value as 16
hexadecimal digits. Optional `senderId` and `durationMs` fields populate the
corresponding protocol fields:

```sh
printf '%s\n' '{"op":"channelValue","channelNumber":0,"senderId":123,"durationMs":5000,"valueHex":"0100000000000000"}' | nc -U /tmp/sd4linux-debug.sock
```

Exactly one of `valueHex`, `relayMode`, or `buttonMode` is required. The result
contains the exact return value from `handleNewValueFromServer()`; `ok` is true
when the handler returns success (`1`).

## Weekly Schedule Configuration

The `weeklySchedule` operation builds a native SUPLA weekly schedule and passes
it to the channel's regular `handleChannelConfig(..., local=true)` handler. This
uses the normal validation and storage lifecycle. Saving a schedule does not
enable weekly schedule mode; use a separate `channelValue` command for that.

Configure Relay channel 0 to be forced off from 08:00 to 19:00, forced on from
19:00 to 21:00, and unconstrained outside those hours on weekdays:

```sh
printf '%s\n' '{"op":"weeklySchedule","channelNumber":0,"programs":[{"mode":"forced_on"},{"mode":"forced_off"}],"entries":[{"days":["mon","tue","wed","thu","fri"],"from":"08:00","to":"19:00","program":2},{"days":["mon","tue","wed","thu","fri"],"from":"19:00","to":"21:00","program":1}]}' | nc -U /tmp/sd4linux-debug.sock
```

Then enable weekly schedule mode:

```sh
printf '%s\n' '{"op":"channelValue","channelNumber":0,"relayMode":6}' | nc -U /tmp/sd4linux-debug.sock
```

For an Action Trigger, program modes are `locked` and `unlocked`:

```sh
printf '%s\n' '{"op":"weeklySchedule","channelNumber":1,"programs":[{"mode":"locked"},{"mode":"unlocked"}],"entries":[{"days":["mon","tue","wed","thu","fri"],"from":"22:00","to":"07:00","program":1},{"days":["mon","tue","wed","thu","fri"],"from":"07:00","to":"22:00","program":2}]}' | nc -U /tmp/sd4linux-debug.sock
printf '%s\n' '{"op":"channelValue","channelNumber":1,"buttonMode":4}' | nc -U /tmp/sd4linux-debug.sock
```

Schedule rules:

- Days are `sun`, `mon`, `tue`, `wed`, `thu`, `fri`, and `sat`.
- Times must use 15-minute boundaries. `24:00` is accepted only as an interval
  end.
- An interval whose end is earlier than its start continues into the next day.
- Later entries overwrite earlier entries where they overlap.
- Program numbers start at `1` and refer to the `programs` array. Up to four
  programs can be defined.
- Relay program modes are `not_set`, `on_once`, `off_once`, `forced_on`,
  `forced_off`, and `automatic`. The target Relay must advertise support for a
  selected mode; in particular, `automatic` requires
  `setAutomaticModeSupported()`. Numeric protocol mode values are also accepted.
- Optional signed 16-bit `value1` and `value2` fields can be added to a program.

Both `programs` and `entries` are required. Empty arrays save a no-op schedule
without enabling weekly schedule mode.

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
printf '%s\n' '{"calcfg":"upsertInstance","instanceId":0,"definitionId":2010,"definitionVersion":1,"revision":1,"paramsJson":"{\"relay.count\":2,\"mode\":\"max\",\"host\":\"192.168.1.50\"}","state":"active"}' | nc -U /tmp/sd4linux-debug.sock
```

List instances:

```sh
printf '%s\n' '{"calcfg":"getInstanceList"}' | nc -U /tmp/sd4linux-debug.sock
```

Inspect an instance. The list contains all instance metadata. Replace `2` with
the instance id returned by the list to fetch its config:

```sh
printf '%s\n' '{"calcfg":"getInstanceConfig","instanceId":2}' | nc -U /tmp/sd4linux-debug.sock
```

Save version 2 of the same definition:

```sh
printf '%s\n' '{"calcfg":"saveDefinition","definitionId":2010,"definitionVersion":2,"definitionJson":"{\"schemaVersion\":1,\"handlerVersion\":1,\"definitionId\":2010,\"definitionVersion\":2,\"maxInstances\":3,\"category\":\"virtual\",\"kind\":\"virtualRelay\",\"parameters\":[{\"key\":\"relay.count\",\"type\":\"uint8\",\"default\":2,\"min\":1,\"max\":4,\"lifecycle\":\"createOnly\",\"affectsTopology\":true},{\"key\":\"startup.delay\",\"type\":\"uint16\",\"default\":5,\"min\":0,\"max\":3600,\"lifecycle\":\"createOnly\"},{\"key\":\"mode\",\"type\":\"enum\",\"default\":\"avg\",\"values\":[\"avg\",\"min\",\"max\"],\"required\":true},{\"key\":\"host\",\"type\":\"string\",\"required\":true},{\"key\":\"display.name\",\"type\":\"string\",\"default\":\"Relay group\"}],\"channels\":[{\"channelId\":1,\"key\":\"relay\",\"kind\":\"virtualRelay\",\"function\":\"powerSwitch\",\"caption\":\"Param relay v2\"}]}"}' | nc -U /tmp/sd4linux-debug.sock
```

Set an existing instance to definition version 2. The same operation handles
ordinary updates and definition-version changes; use a newer revision:

```sh
printf '%s\n' '{"calcfg":"upsertInstance","instanceId":1,"definitionId":2010,"definitionVersion":2,"revision":2,"paramsJson":"{\"relay.count\":2,\"startup.delay\":5,\"mode\":\"max\",\"host\":\"192.168.1.50\",\"display.name\":\"Relay group v2\"}"}' | nc -U /tmp/sd4linux-debug.sock
```

For a config-only update that keeps the existing artifact, add
`"keepArtifact":true`. Omitting it with no artifact data removes the artifact.

Inspect the upgraded instance:

```sh
printf '%s\n' '{"calcfg":"getInstanceList"}' | nc -U /tmp/sd4linux-debug.sock
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
```

## Direct Suplet Command JSON

The socket also accepts the direct Suplet command JSON handled by
`SuplaDevice.validateSupletCommandJson()` and
`SuplaDevice.applySupletCommandJson()`. Use CALCFG examples above when testing
the protocol-shaped path. Use direct command JSON when testing the internal
server-config handler contract.
