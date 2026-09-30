#!/usr/bin/env python3
# SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise two ordinary sd4linux processes with SRPC unavailable.

Requires a working local multicast interface. This is a local integration
check, not the real Server/ESP32 final PoC2 acceptance procedure.
"""

import argparse
import pathlib
import re
import socket
import subprocess
import tempfile
import time


def command(path, line):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.settimeout(2)
                client.connect(str(path))
                client.sendall((line + "\n").encode())
                data = b""
                while b"\n" not in data:
                    part = client.recv(4096)
                    if not part:
                        break
                    data += part
                result = data.decode()
                if '"error":"busy"' not in result:
                    return result
        except (FileNotFoundError, ConnectionRefusedError):
            pass
        time.sleep(0.1)
    raise RuntimeError("debug socket unavailable: " + str(path))


def wait_for(check, description, processes, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if any(process.poll() is not None for process in processes):
            raise RuntimeError("sd4linux exited: " + description)
        if check():
            print("PASS:", description, flush=True)
            return
        time.sleep(0.1)
    raise RuntimeError("timed out: " + description)


def counters(path):
    return {key: int(value) for key, value in re.findall(
        r"(\w+)=(\d+)", command(path, "show-counters"))}


def transport_is_disabled(path):
    status = command(path, "show-status")
    return ("transport=closed" in status and "transport_enabled=0" in status
            and "srpc_registered=0" in status)


def transport_is_enabled(path):
    status = command(path, "show-status")
    return ("transport=open" in status and "transport_enabled=1" in status
            and "srpc_registered=0" in status)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=pathlib.Path)
    parser.add_argument("--port-a", type=int, default=2017)
    parser.add_argument("--port-b", type=int, default=2018)
    args = parser.parse_args()
    if not (0 < args.port_a <= 65535 and 0 < args.port_b <= 65535
            and args.port_a != args.port_b):
        parser.error("ports must be different and within 1..65535")
    binary = args.binary.resolve(strict=True)
    run = pathlib.Path(tempfile.mkdtemp(prefix="poc2-smoke-", dir="/tmp/codex"))
    print("Artifacts:", run, flush=True)
    processes = []
    logs = []
    sockets = {}
    try:
        for role, port in (("A", args.port_a), ("B", args.port_b)):
            state = run / role
            state.mkdir()
            channel = ("  - type: VirtualRelay\n" if role == "A" else
                       "  - type: ActionTriggerParsed\n    name: poc2-event\n")
            config = run / (role + ".yaml")
            config.write_text(
                f"name: SupLAN PoC2 smoke {role}\nstate_files_path: {state}\n"
                "security_level: 0\nsupla:\n  server: 127.0.0.2\n"
                "  mail: poc2-smoke@example.invalid\nchannels:\n" + channel +
                f"suplan:\n  enabled: true\n  role: {role}\n"
                f"  unicast_port: {port}\n")
            sockets[role] = run / (role + ".sock")
            log = (run / (role + ".log")).open("w")
            logs.append(log)
            processes.append(subprocess.Popen(
                [str(binary), "--config", str(config), "--debug-socket",
                 str(sockets[role]), "--debug-log-port", "0"],
                stdout=log, stderr=subprocess.STDOUT))
        a, b = sockets["A"], sockets["B"]
        for role, path in sockets.items():
            wait_for(lambda: "transport=open" in command(path, "show-status"),
                     role + " opens SupLAN in normal lifecycle", processes)

        assert "OK" in command(b, "read 50001")
        wait_for(lambda: counters(b)["states_rx"] >= 1,
                 "READ reaches the real relay", processes)
        for value in (1, 0):
            before = counters(a)["controls"]
            assert "OK" in command(b, f"control 50001 {value}")
            wait_for(lambda: counters(a)["controls"] == before + 1 and
                     f"50001=relay:{value}" in command(a, "show-resources"),
                     f"CONTROL {value} changes the ordinary Element once",
                     processes)

        before = counters(b)["states_rx"]
        assert "OK" in command(a, "set-resource-value 50001 1")
        wait_for(lambda: counters(b)["states_rx"] > before,
                 "local channel update reaches interested peer", processes)

        established = counters(a)["session_established"]
        assert "OK" in command(a, "forget-session 0")
        before = counters(b)["states_rx"]
        assert "OK" in command(b, "read 50001")
        wait_for(lambda: counters(a)["session_established"] > established and
                 counters(b)["states_rx"] > before,
                 "READ recovers session without SRPC registration", processes)

        assert "OK" in command(a, "transport off")
        wait_for(lambda: transport_is_disabled(a),
                 "SupLAN transport can be disabled independently", processes)
        assert "OK" in command(a, "transport on")
        wait_for(lambda: transport_is_enabled(a),
                 "SupLAN transport recovers without restarting SRPC", processes)
        before = counters(b)["states_rx"]
        assert "OK" in command(b, "read 50001")
        wait_for(lambda: counters(b)["states_rx"] > before,
                 "READ works again after transport recovery", processes)

        before = counters(b)["reads"]
        assert "OK" in command(a, "read 50002")
        wait_for(lambda: counters(b)["reads"] > before,
                 "READ establishes Action Trigger interest", processes)
        before = counters(a)["actions_rx"]
        assert "OK" in command(b, "emit-action 50002 1")
        wait_for(lambda: counters(a)["actions_rx"] > before,
                 "real Action Trigger channel sends an event", processes)

        for role, path in sockets.items():
            status = command(path, "show-status")
            assert "srpc_connected=0" in status
            assert "srpc_registered=0" in status
            assert "transport_enabled=1" in status
            assert "transport=open" in status
            print(role, status.strip())
            print(role, command(path, "show-counters").strip())
            print(role, command(path, "show-pools").strip())
        print("POC2_LOCAL_SMOKE=PASS (real Server/ESP32 acceptance NOT RUN)")
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        for log in logs:
            log.close()


if __name__ == "__main__":
    main()
