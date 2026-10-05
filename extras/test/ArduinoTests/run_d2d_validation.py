#!/usr/bin/env python3
# SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later
"""Test ordinary sd4linux (PoC role B) against Arduino SupLanD2D role A.

Requires pyserial and a flashed/configured SupLanD2D fixture. Deterministic PoC
peers are test credentials. The ESP device keeps its normal Cloud configuration;
the temporary Linux device deliberately has no working Server connection.

The LAN must permit multicast discovery and UDP replies to Linux port 2018.
For hosts with multiple interfaces, select the LAN address with --bind-address.
Default tests retain the ESP radio sleep setting; --no-sleep is diagnostic only.
"""

import argparse
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--port', default='/dev/ttyUSB0')
    parser.add_argument('--bind-address', default='0.0.0.0')
    parser.add_argument('--no-sleep', action='store_true')
    parser.add_argument('--reverse-first', action='store_true')
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    os.umask(0o077)
    root = Path(tempfile.mkdtemp(prefix='suplan-arduino-d2d-test-',
                                 dir='/tmp/codex'))
    print('Artifacts:', root, flush=True)
    (root / 'state').mkdir()
    (root / 'tmp').mkdir()
    config = root / 'B.yaml'
    config.write_text(
        'name: SupLAN Arduino D2D Linux peer\nstate_files_path: ' +
        str(root / 'state') + '\nsecurity_level: 0\nsupla:\n' +
        '  server: 127.0.0.2\n  mail: d2d@example.invalid\nchannels:\n' +
        '  - type: ActionTriggerParsed\n    name: d2d-event\nsuplan:\n' +
        '  enabled: true\n  role: B\n  unicast_port: 2018\n' +
        '  bind_address: ' + args.bind_address + '\n', encoding='utf-8')
    debug_socket = root / 'B.sock'
    uart = serial.Serial(port=None, baudrate=115200, timeout=0.05)
    uart.dtr = False
    uart.rts = False
    uart.port = args.port
    samples, diagnostics, results = [], [], []
    last, d2d, flags = {}, {}, {}
    process = None

    def save():
        (root / 'results.json').write_text(json.dumps(
            {'results': results, 'samples': samples,
             'diagnostics': diagnostics, 'startup': flags}, indent=2) + '\n',
            encoding='utf-8')

    def command(line):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.settimeout(2)
                client.connect(str(debug_socket))
                client.sendall((line + "\n").encode())
                data = b""
                while b"\n" not in data:
                    part = client.recv(4096)
                    if not part:
                        break
                    data += part
                response = data.decode("utf-8", "replace")
            with (root / "debug-responses.log").open("a") as log:
                log.write(line + " -> " + response)
            if '"error":"busy"' not in response:
                return response
            time.sleep(0.1)
        raise RuntimeError("debug socket remains busy: " + line)

    def counters():
        return {key: int(value) for key, value in re.findall(
            r'(\w+)=(\d+)', command('show-counters'))}

    def send(line):
        uart.write((line + '\n').encode())
        uart.flush()

    def state_since(offset, value):
        with (root / 'linux.log').open(encoding='utf-8') as log:
            log.seek(offset)
            return re.search(r'SupLAN STATE .*resource=50001 .*value0=' +
                             str(value) + r'\b', log.read()) is not None

    with (root / 'uart-private.log').open('w', encoding='utf-8') as raw, \
            (root / 'linux.log').open('w', encoding='utf-8') as linux_log:
        def pump(seconds=0.25):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                line = uart.readline().decode('utf-8', 'replace').strip()
                if not line:
                    continue
                raw.write(line + '\n')
                raw.flush()
                if not line.startswith('VALIDATION'):
                    continue
                print(line, flush=True)
                values = {key: int(value) for key, value in
                          re.findall(r'(\w+)=(-?\d+)', line)}
                if line.startswith('VALIDATION SAMPLE '):
                    last.update(values)
                    samples.append(values)
                elif line.startswith('VALIDATION D2D '):
                    d2d.update(values)
                    diagnostics.append(values)
                elif line.startswith('VALIDATION D2D_CRYPTO '):
                    flags['crypto'] = values.get('ok')
                elif line.startswith('VALIDATION ACTION '):
                    flags['last_action'] = values
                elif line.startswith('VALIDATION D2D_KDF '):
                    flags['kdf'] = values.get('ok')
                elif line.startswith('VALIDATION D2D_INIT '):
                    flags['d2d_init'] = values.get('ok')
                    flags['boot_count'] = flags.get('boot_count', 0) + 1
                    last.clear()
                    d2d.clear()
                elif line.startswith('VALIDATION INIT '):
                    flags['init'] = values.get('ok')
                    flags['advertised'] = values.get('flag')
                elif line.startswith('VALIDATION PROTO '):
                    flags['proto'] = values.get('ok')

        def wait(check, name, timeout=30):
            end = time.monotonic() + timeout
            next_sample = 0
            while time.monotonic() < end:
                if process.poll() is not None:
                    raise RuntimeError('sd4linux exited')
                if time.monotonic() >= next_sample:
                    send('sample')
                    send('d2d-status')
                    next_sample = time.monotonic() + 1
                pump()
                try:
                    if check():
                        results.append({'name': name, 'pass': True,
                                        'sample': dict(last),
                                        'source': dict(d2d)})
                        print('PASS:', name, flush=True)
                        save()
                        return
                except (FileNotFoundError, ConnectionRefusedError):
                    pass
            raise RuntimeError('timeout: ' + name)

        def control(value, name):
            before = d2d['controls']
            before_ack = counters()['ack_rx']
            offset = (root / 'linux.log').stat().st_size
            if 'OK' not in command('control 50001 ' + str(value)):
                raise RuntimeError('control rejected')
            wait(lambda: last.get('relay') == value and
                 d2d.get('controls') == before + 1 and
                 counters()['ack_rx'] > before_ack and
                 state_since(offset, value), name)

        try:
            uart.open()
            process = subprocess.Popen(
                [str(binary), '--config', str(config), '--debug-socket',
                 str(debug_socket), '--debug-log-port', '0'],
                stdout=linux_log, stderr=subprocess.STDOUT,
                env=dict(os.environ, TMPDIR=str(root / 'tmp')))
            wait(lambda: flags.get('crypto') == 1 and
                 flags.get('d2d_init') == 1 and flags.get('proto') == 1 and
                 flags.get('advertised') == 0 and last.get('ready') == 1 and
                 d2d.get('transport') == 1 and
                 'transport=open' in command('show-status'),
                 'ESP crypto vectors, Cloud ready and both D2D transports', 180)
            if args.no_sleep:
                send('d2d-no-sleep')
                pump(1)
            if args.reverse_first:
                before = counters()['reads']
                send('d2d-read-action')
                wait(lambda: counters()['reads'] > before,
                     'ESP multicast LOCATE, SESSION and event-only READ')
                before = d2d.get('actions_rx', 0)
                if 'OK' not in command('emit-action 50002 1'):
                    raise RuntimeError('action rejected')
                wait(lambda: d2d.get('actions_rx', 0) > before and
                     flags.get('last_action') == {'resource': 50002, 'action': 2},
                     'Linux ActionTrigger event reaches ESP over protected DATA')
            send('relay-off')
            pump(2)
            before = counters()['states_rx']
            offset = (root / 'linux.log').stat().st_size
            if 'OK' not in command('read 50001'):
                raise RuntimeError('read rejected')
            wait(lambda: counters()['states_rx'] > before and
                 state_since(offset, 0) and d2d.get('reads', 0) > 0 and
                 d2d.get('session_established', 0) > 0,
                 'multicast LOCATE, SESSION and READ relay OFF')
            control(1, 'CONTROL ON changes Channel 7, ACK and STATE')
            control(0, 'CONTROL OFF changes Channel 7, ACK and STATE')
            before = counters()['states_rx']
            offset = (root / 'linux.log').stat().st_size
            send('relay-on')
            wait(lambda: last.get('relay') == 1 and
                 counters()['states_rx'] > before and state_since(offset, 1),
                 'local relay change publishes STATE to Linux')
            established = d2d['session_established']
            before = counters()['states_rx']
            send('d2d-forget')
            pump(1)
            command('forget-session 0')
            command('clear-endpoint 0')
            if 'OK' not in command('read 50001'):
                raise RuntimeError('recovery read rejected')
            wait(lambda: d2d.get('session_established', 0) > established and
                 counters()['states_rx'] > before,
                 'fresh LOCATE and SESSION after forgetting both sessions')
            send('reconnect')
            pump(1)
            wait(lambda: last.get('ready') == 1,
                 'Cloud registration recovers after SRPC disconnect', 90)
            control(0, 'D2D CONTROL works after SRPC reconnect')
            send('d2d-off')
            wait(lambda: d2d.get('transport') == 0 and last.get('ready') == 1,
                 'D2D transport stops while Cloud stays registered')
            send('d2d-on')
            wait(lambda: d2d.get('transport') == 1,
                 'D2D transport reopens')
            before = counters()['states_rx']
            if 'OK' not in command('read 50001'):
                raise RuntimeError('transport recovery read rejected')
            wait(lambda: counters()['states_rx'] > before,
                 'READ recovers after D2D transport restart')
            before_boot = flags.get('boot_count', 0)
            send('restart')
            wait(lambda: last.get('ready') == 1 and
                 flags.get('boot_count', 0) > before_boot and
                 d2d.get('transport') == 1,
                 'ESP reboot restores config and D2D fixture', 180)
            if args.no_sleep:
                send('d2d-no-sleep')
                pump(1)
            before = counters()['states_rx']
            if 'OK' not in command('read 50001'):
                raise RuntimeError('reboot read rejected')
            wait(lambda: counters()['states_rx'] > before,
                 'Linux READ recovers after ESP reboot')
            control(1, 'CONTROL ON after ESP reboot')
            control(0, 'CONTROL OFF after ESP reboot')
            pump(6)
            print('LINUX', command('show-status').strip(), flush=True)
            print('LINUX', command('show-counters').strip(), flush=True)
            print('LINUX', command('show-pools').strip(), flush=True)
            send('sample')
            send('d2d-status')
            pump(2)
        except Exception as error:
            results.append({'name': 'failure', 'pass': False, 'error': str(error)})
            print('FAIL:', error, flush=True)
        finally:
            if uart.is_open:
                send('relay-off')
                pump(0.5)
                uart.close()
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            save()
    return int(any(not result['pass'] for result in results))


if __name__ == '__main__':
    raise SystemExit(main())
