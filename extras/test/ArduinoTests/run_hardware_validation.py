#!/usr/bin/env python3
# SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise an already configured ServerBootstrap fixture over UART.

Requires pyserial. Changes VirtualRelay state, reconnects SRPC/Wi-Fi and restarts
once; preserves configuration. Does not inject synthetic SERVER identities.
Opening UART can itself reset some USB boards. Private UART logs stay in the
unique artifact directory; the console only reports fixture telemetry.
"""

import argparse
import json
import os
from pathlib import Path
import re
import tempfile
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='/dev/ttyUSB0')
    parser.add_argument('--cycles', type=int, default=10)
    args = parser.parse_args()
    if args.cycles < 1:
        parser.error('--cycles must be positive')
    os.umask(0o077)
    base = Path(tempfile.mkdtemp(prefix='suplan-arduino-hardware-',
                                 dir='/tmp/codex'))
    print('Artifacts:', base, flush=True)
    uart = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    uart.dtr = False
    uart.rts = False
    uart.port = args.port
    samples, results = [], []

    def save():
        (base / 'results.json').write_text(
            json.dumps({'results': results, 'samples': samples}, indent=2) + '\n',
            encoding='utf-8')

    def check(name, success, sample=None):
        results.append({'name': name, 'pass': bool(success), 'sample': sample})
        print('CHECK', name, 'PASS' if success else 'FAIL', flush=True)
        save()
        if not success:
            raise RuntimeError(name)

    def send(command):
        uart.write((command + '\n').encode())
        uart.flush()

    with (base / 'uart-private.log').open('w', encoding='utf-8') as raw:
        def read(seconds, predicate=None):
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
                if line.startswith('VALIDATION SAMPLE'):
                    sample = {key: int(value) for key, value in
                              re.findall(r'(\w+)=(-?\d+)', line)}
                    samples.append(sample)
                    if predicate and predicate(sample):
                        return sample
            return None

        def ready(sample):
            return sample['ready'] == 1 and sample['wifi'] == 1

        try:
            uart.open()
            send('sample')
            initial = read(90, ready)
            check('initial_cloud_registration', initial, initial)
            read(12)
            for command, value in [('relay-on', 1), ('relay-off', 0)]:
                send(command)
                send('sample')
                sample = read(15, lambda data: ready(data) and
                              data['relay'] == value)
                check(command, sample, sample)
            for cycle in range(args.cycles):
                before = samples[-1]
                send('reconnect')
                send('sample')
                down = read(15, lambda data: data['ready'] == 0)
                up = read(75, ready)
                check('srpc_reconnect_' + str(cycle + 1),
                      down and up and up['ms'] > before['ms'], up)
                read(6)
            send('wifi-reconnect')
            send('sample')
            down = read(20, lambda data: data['ready'] == 0)
            up = read(100, ready)
            check('wifi_reconnect', down and up, up)
            read(8)
            before = samples[-1]
            send('restart')
            up = read(100, lambda data: ready(data) and
                      data['ms'] < before['ms'])
            check('restart_config_persisted', up, up)
            read(12)
            send('sample')
            read(2)
        except Exception as error:
            results.append({'name': 'harness_exception', 'pass': False,
                            'error': str(error)})
            print('FAIL:', error, flush=True)
        finally:
            uart.close()
            save()
    return int(any(not result['pass'] for result in results))


if __name__ == '__main__':
    raise SystemExit(main())
