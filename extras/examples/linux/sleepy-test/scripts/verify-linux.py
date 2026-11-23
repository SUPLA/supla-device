#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Read-only Destination checks while run.sh restarts the real Source."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import time


def query(path, command):
    # The SDK closes the previous client's slot on the next iterate.
    for _ in range(5):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(1)
            client.connect(str(path))
            client.sendall((command + "\n").encode())
            with client.makefile() as stream:
                line = stream.readline()
        if '"error":"busy"' not in line:
            return dict(re.findall(r"(\w+)=([^\s]+)", line))
        time.sleep(0.03)
    raise RuntimeError("Destination debug socket remained busy")


def boot_ms():
    return time.clock_gettime_ns(time.CLOCK_BOOTTIME) // 1_000_000


def fault_slots(plan, mode, slots):
    if plan["mode"] != mode or int(plan["slots"]) != slots:
        raise RuntimeError("Supervisor mode or slot count differs from request")
    if mode == "normal":
        return range(0)
    first = int(plan["fault_slot"])
    count = int(plan["skip_slots"]) if mode == "skip" else 1
    after = first + count
    if first <= 1 or after > slots:
        raise RuntimeError("Provide a normal slot before and after the complete fault")
    return range(first, after)


def quiet_window(slot, cycle, period_ms):
    result = slot.get("CYCLE_RESULT")
    sleep = cycle.get("ANNOUNCE_SLEEP")
    if not result or not sleep or cycle.get("SLEEP_RESULT", {}).get("sent") != "1":
        return None
    start = int(result["boot_ms"]) + 150
    end = min(int(slot["APP_STARTED"]["planned_ms"]) + period_ms,
              int(sleep["boot_ms"]) + int(sleep["duration_sec"]) * 1000) - 150
    return start, end


def check_run(slots, cycles, plan, mode, slot_count, seen, expired, quiet,
              expect_expiry):
    faults = fault_slots(plan, mode, slot_count)
    period_ms = int(plan["period_ms"])
    measurements = []
    quiet_ms = {}
    killed = 0
    for number in range(1, slot_count + 1):
        record = slots[number]
        skipped = record.get("SLOT_SKIPPED")
        if number in faults and mode == "skip":
            if not skipped or skipped.get("reason") != "injected" or "APP_STARTED" in record:
                raise RuntimeError(f"Slot {number} did not produce its planned skip")
            continue
        if skipped or "APP_STARTED" not in record or "CYCLE_RESULT" not in record:
            raise RuntimeError(f"Slot {number} missing its planned process result")

        launch = record["APP_STARTED"]
        result = record["CYCLE_RESULT"]
        cycle = int(launch["cycle"])
        if result["cycle"] != launch["cycle"]:
            raise RuntimeError(f"Slot {number} recorded inconsistent cycles")
        events = cycles[cycle]
        if number in faults:
            if (result["code"], result["outcome"]) != ("137", "kill"):
                raise RuntimeError(f"Slot {number} did not produce its planned SIGKILL")
            if "ANNOUNCE_SLEEP" in events:
                raise RuntimeError(f"Cycle {cycle} announced sleep before SIGKILL")
            killed += 1
            continue

        if (result["code"], result["outcome"]) != ("0", "normal") or (
                "STATE_SENT" not in events or "READ_RECEIVED" not in events):
            raise RuntimeError(f"Source cycle {cycle} did not send a READ snapshot and exit normally")
        if events.get("SLEEP_RESULT", {}).get("sent") != "1":
            raise RuntimeError(f"Source cycle {cycle} had no transmitted sleep announcement")
        started = int(launch["boot_ms"])
        ended = int(result["boot_ms"])
        value = 20 + ((cycle - 1) % 4) * 0.5
        if not any(started <= ts <= ended + 1500 and temp == value
                   for ts, temp in seen.values()):
            raise RuntimeError(
                f"Cycle {cycle} missing fresh Destination HVAC value {value}; "
                f"observed={[temp for _, temp in seen.values()]}"
            )
        measurements.append(value)

        # The supervisor exits after the final slot, before a complete sleep.
        if number == slot_count:
            continue
        start, end = quiet_window(record, events, period_ms)
        samples = quiet.get(number, [])
        if len(samples) < 2 or samples[-1][0] - samples[0][1] < (end - start) / 2:
            raise RuntimeError(f"Slot {number} has insufficient coverage of announced sleep")
        if any(sample[2] != samples[0][2] for sample in samples[1:]):
            raise RuntimeError(f"Destination sent LOCATE or DATA during sleep after slot {number}")
        quiet_ms[number] = samples[-1][0] - samples[0][1]

    expiry_seen = bool(expired)
    if faults:
        first_fault = slots[faults.start]
        if mode == "skip":
            gap_start = int(first_fault["SLOT_SKIPPED"]["planned_ms"])
        else:
            # The supervisor reads /proc/uptime, truncated to centiseconds.
            gap_start = int(first_fault["CYCLE_RESULT"]["boot_ms"]) + 10
        restart = int(slots[faults.stop]["APP_STARTED"]["launch_before_ms"])
        expiry_seen = any(gap_start <= start <= end < restart
                          for start, end in expired)
    if expect_expiry and not expiry_seen:
        raise RuntimeError("Destination did not expose cache expiry during the injected gap")
    return measurements, killed, expiry_seen, quiet_ms


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("socket", type=Path, help="Destination's existing Linux debug socket")
    ap.add_argument("source_id", type=int, help="SERVER channel ID of the thermometer")
    ap.add_argument("hvac_channel", type=int, help="local Destination HVAC channel number")
    ap.add_argument("destination_pid", type=int, help="PID of the Destination Linux process")
    ap.add_argument("--mode", choices=("normal", "kill", "skip"), default="normal")
    ap.add_argument("--slots", type=int, default=20)
    ap.add_argument("--expect-expiry", action="store_true")
    ap.add_argument("--source-udp-port", type=int,
                    help="Verify UDP closure in the isolated Source network namespace")
    args = ap.parse_args()
    if args.slots < 2:
        ap.error("--slots must be at least 2")
    if args.expect_expiry and args.mode == "normal":
        ap.error("--expect-expiry requires --mode skip or kill")

    root = Path(__file__).resolve().parents[1]
    stats = Path(f"/proc/{args.destination_pid}/status")

    def get_state():
        return query(args.socket, f"remote-state {args.source_id}")

    baseline_state = get_state()
    if baseline_state.get("id") != str(args.source_id):
        raise SystemExit("Destination debug socket or Source channel ID invalid")

    runner = subprocess.Popen(
        [root / "scripts/run.sh", args.mode, str(args.slots)], cwd=root,
        stdout=subprocess.PIPE, text=True,
    )
    seen = {}
    rss = []
    pool_max = 0
    expired = []
    available = None
    quiet = defaultdict(list)
    checked_closed = set()
    slots = defaultdict(dict)
    cycles = defaultdict(dict)
    plan = {}
    last_launch = 0
    try:
        path = runner.stdout.readline().strip()
        if not path:
            raise RuntimeError("Source supervisor did not start")
        print(f"Source log: {root / path}", flush=True)
        with (root / path).open() as logfile:
            finished = False
            while True:
                chunk = logfile.readlines()
                for line in chunk:
                    if not line.startswith(("SLEEPY ", "RUNNER ")):
                        continue
                    fields = dict(re.findall(r"(\w+)=([^\s]+)", line))
                    event = fields["event"]
                    if line.startswith("SLEEPY "):
                        cycles[int(fields["cycle"])][event] = fields
                    elif event == "RUN_START":
                        plan = fields
                        fault_slots(plan, args.mode, args.slots)
                    else:
                        number = int(fields["slot"])
                        slots[number][event] = fields
                        if event == "APP_STARTED":
                            last_launch = number

                state_started = boot_ms()
                state = get_state()
                hvac = query(args.socket, f"hvac-config {args.hvac_channel}")
                state_finished = boot_ms()
                if hvac.get("main") == str(args.source_id):
                    if state.get("usable") == "1":
                        # A stale cache cannot satisfy a new cycle after wrap
                        # of the four-value sequence. Ignore provisioning.
                        received = state["received_ms"]
                        temp = struct.unpack("<d", bytes.fromhex(state["value"]))[0]
                        if hvac.get("temp") == str(round(temp * 100)):
                            available = True
                            if received != baseline_state.get("received_ms"):
                                seen.setdefault(received, (boot_ms(), temp))
                    elif state.get("usable") == "0" and hvac.get("temp") == "-32768":
                        if available:
                            expired.append((state_started, state_finished))
                        available = False
                status = query(args.socket, "show-status")
                pool_max = max(pool_max, int(status["sessions"]))
                rss.append(int(re.search(r"^VmRSS:\s+(\d+)", stats.read_text(), re.M)[1]))

                if 0 < last_launch < args.slots:
                    slot = slots[last_launch]
                    launch = slot["APP_STARTED"]
                    period_ms = int(plan["period_ms"])
                    end_slot = int(launch["planned_ms"]) + period_ms - 150
                    result = slot.get("CYCLE_RESULT")
                    now = boot_ms()
                    if result and int(result["boot_ms"]) + 150 <= now <= end_slot:
                        pid = launch["pid"]
                        if Path(f"/proc/{pid}").exists():
                            raise RuntimeError(f"Source PID {pid} survived slot {last_launch}")
                        if args.source_udp_port is not None:
                            port = f":{args.source_udp_port:04X}"
                            rows = Path("/proc/net/udp").read_text().splitlines()[1:]
                            if any(row.split()[1].endswith(port) for row in rows):
                                raise RuntimeError(f"Source UDP is open during sleep after {last_launch}")
                            checked_closed.add(last_launch)
                        window = quiet_window(slot, cycles[int(launch["cycle"])], period_ms)
                        if window:
                            start, end = window
                            sampled_from = boot_ms()
                            if start <= sampled_from <= end:
                                counters = query(args.socket, "show-counters")
                                sampled_until = boot_ms()
                                if sampled_until <= end:
                                    quiet[last_launch].append((sampled_from, sampled_until,
                                        (counters["locate_tx"], counters["data_tx"])))
                if finished:
                    break
                finished = runner.poll() is not None
                if finished and runner.returncode:
                    raise RuntimeError(f"Source supervisor exited {runner.returncode}")
                time.sleep(0.15)
    finally:
        if runner.poll() is None:
            runner.terminate()
            runner.wait(timeout=8)
        runner.stdout.close()

    if not plan or "RUN_END" not in slots[args.slots]:
        raise RuntimeError("Source supervisor did not complete all planned slots")
    measurements, abnormal, saw_expiry, quiet_ms = check_run(
        slots, cycles, plan, args.mode, args.slots, seen, expired, quiet,
        args.expect_expiry)
    if args.source_udp_port is not None and not checked_closed:
        raise RuntimeError("No sleep interval sampled for Source UDP closure")
    if pool_max != 1:
        raise RuntimeError(f"Expected one live session for the isolated pair, got max {pool_max}")
    # Broad allowance for allocator/OS accounting after the first few wakes.
    rss_baseline = max(rss[: min(40, len(rss))])
    if max(rss) - rss_baseline > 4096:
        raise RuntimeError("Destination RSS increased by more than 4 MiB")
    print(json.dumps({
        "status": "PASS", "normal_cycles": len(measurements), "killed": abnormal,
        "destination_values": measurements, "destination_expired": saw_expiry,
        "sleep_intervals_sampled": len(quiet_ms),
        "sleep_compared_ms_by_slot": quiet_ms,
        "sleep_sockets_checked": len(checked_closed),
        "session_peak": pool_max, "rss_baseline_kib": rss_baseline,
        "rss_peak_kib": max(rss), "rss_final_kib": rss[-1],
    }, indent=2))


if __name__ == "__main__":
    main()
