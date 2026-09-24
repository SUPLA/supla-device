#!/usr/bin/env python3
"""Linux-to-Linux PoC1 acceptance run using local UDP unicast ports."""

import argparse
import os
import re
import selectors
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--bind-a", default="127.0.0.1")
    parser.add_argument("--bind-b", default="127.0.0.1")
    parser.add_argument("--port-a", default="2017")
    parser.add_argument("--port-b", default="2018")
    args = parser.parse_args()

    if args.bind_a == args.bind_b and args.port_a == args.port_b:
        parser.error("same-host instances need distinct --port-a/--port-b")

    selector = selectors.DefaultSelector()
    processes = {}
    logs = {"A": [], "B": []}
    partial = {"A": b"", "B": b""}
    os.makedirs("/tmp/codex", exist_ok=True)
    config_dir = tempfile.TemporaryDirectory(
        prefix="suplan-poc1-acceptance-", dir="/tmp/codex")
    config_a = os.path.join(config_dir.name, "a.yaml")
    config_b = os.path.join(config_dir.name, "b.yaml")
    with open(config_a, "w", encoding="utf-8") as config_file:
        config_file.write("suplan:\n  unicast_port: 2016\n")
    with open(config_b, "w", encoding="utf-8") as config_file:
        config_file.write(f"suplan:\n  unicast_port: {args.port_b}\n")

    def start(role, bind, config, cli_port=None):
        command = [args.binary, "--role", role, "--bind", bind,
                   "--config", config]
        if cli_port is not None:
            command.extend(["--suplan-port", cli_port])
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, bufsize=0)
        processes[role] = process
        selector.register(process.stdout, selectors.EVENT_READ, role)

    def drain(timeout, predicate=None):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for key, _ in selector.select(min(0.05, deadline - time.monotonic())):
                role = key.data
                data = os.read(key.fileobj.fileno(), 4096)
                if not data:
                    continue
                partial[role] += data
                while b"\n" in partial[role]:
                    line, partial[role] = partial[role].split(b"\n", 1)
                    decoded = line.decode("utf-8", errors="replace")
                    logs[role].append(decoded)
                    print(f"[{role}] {decoded}", flush=True)
            if predicate is not None and predicate():
                return True
        return predicate() if predicate is not None else False

    def send(role, command):
        process = processes[role]
        process.stdin.write((command + "\n").encode("ascii"))
        process.stdin.flush()

    def seen(role, text, after=0):
        return any(text in line for line in logs[role][after:])

    def counters(role):
        before = len([line for line in logs[role]
                      if line.startswith("COUNTERS ")])
        send(role, "show-counters")
        if not drain(2, lambda: len([line for line in logs[role]
                                     if line.startswith("COUNTERS ")]) > before):
            raise RuntimeError(f"FAIL: {role} counters did not respond")
        rows = [line for line in logs[role]
                if line.startswith("COUNTERS ")]
        return {name: int(value) for name, value in
                re.findall(r"(\w+)=(\d+)", rows[-1])}

    def pools(role):
        before = len([line for line in logs[role] if line.startswith("POOLS")])
        send(role, "show-pools")
        if not drain(2, lambda: len([line for line in logs[role]
                                     if line.startswith("POOLS")]) > before):
            raise RuntimeError(f"FAIL: {role} pool diagnostics did not respond")
        rows = [line for line in logs[role] if line.startswith("POOLS")]
        result = {name: (int(used), int(maximum), int(high)) for name,
                  used, maximum, high in re.findall(
                      r"(\w+)=(\d+)/(\d+)\(high=(\d+)\)", rows[-1])}
        workspace = re.search(r"workspace_bytes=(\d+)", rows[-1])
        result["workspace_bytes"] = int(workspace.group(1)) if workspace else 0
        return result

    def event_count(role, marker):
        return sum(marker in line for line in logs[role])

    def control_and_ack(value, timeout=3):
        drain(0.03)
        controls_before = event_count("A", "CONTROL resource=50001")
        acks_before = event_count("B", "ACK resource=50001")
        send("B", f"control 50001 {value}")
        if not drain(timeout, lambda: event_count(
                "A", "CONTROL resource=50001") == controls_before + 1 and
                event_count("B", "ACK resource=50001") >= acks_before + 1):
            raise RuntimeError("FAIL: CONTROL did not receive one ACK")

    def require(condition, description):
        if not condition:
            raise RuntimeError("FAIL: " + description)
        print("PASS: " + description, flush=True)

    try:
        start("A", args.bind_a, config_a, args.port_a)
        start("B", args.bind_b, config_b)
        require(drain(3, lambda: seen("A", "READY role=A") and
                      seen("B", "READY role=B") and
                      seen("A", f"bind={args.bind_a}:{args.port_a}") and
                      seen("B", f"bind={args.bind_b}:{args.port_b}")),
                "YAML unicast port and CLI override start both Linux harnesses")

        send("A", "multicast-self-test")
        send("B", "multicast-self-test")
        require(drain(4, lambda: seen("A", "Local multicast receive  PASS") and
                      seen("B", "Local multicast receive  PASS")),
                "local multicast self-test passes on both Linux instances")

        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "authenticated LOCATE, SESSION and current-state READ")

        send("B", "control 50001 1")
        require(drain(4, lambda: seen("A", "CONTROL resource=50001 value=1") and
                      seen("B", "ACK resource=50001")), "CONTROL ON and ACK")
        send("B", "control 50001 0")
        require(drain(4, lambda: seen("A", "CONTROL resource=50001 value=0") and
                      len([line for line in logs["B"]
                           if "ACK resource=50001" in line]) >= 2),
                "CONTROL OFF and ACK")

        state_count = len([line for line in logs["B"]
                           if "STATE resource=50001" in line])
        send("A", "set-resource-value 50001 1")
        require(drain(4, lambda: len([line for line in logs["B"]
                                      if "STATE resource=50001" in line]) >
                      state_count), "independent local state notification")

        controls_before = event_count("A", "CONTROL resource=50001")
        acks_before = event_count("B", "ACK resource=50001")
        retry_before = counters("B")["retry_tx"]
        duplicate_before = counters("A")["control_duplicates"]
        send("B", "capture-next-data compare-retry")
        drain(0.1)
        send("A", "drop-next-tx ACK")
        send("B", "control 50001 1")
        require(drain(5, lambda: len([line for line in logs["A"]
                                      if "CONTROL resource=50001" in line]) ==
                      controls_before + 1 and
                      len([line for line in logs["B"]
                           if "ACK resource=50001" in line]) > acks_before),
                "lost ACK retries identical CONTROL without duplicate effect")
        send("B", "show-data-comparison")
        require(drain(1, lambda: seen("B", "DATA_COMPARE=PASS")),
                "ACK retry retransmits byte-identical protected DATA")
        a_after_retry = counters("A")
        b_after_retry = counters("B")
        require(b_after_retry["retry_tx"] > retry_before and
                a_after_retry["control_duplicates"] > duplicate_before,
                "receiver records duplicate CONTROL suppression")

        b0 = len(logs["B"])
        send("B", "forget-session 0")
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "session loss recovers without deleting authorization or interest")

        bad_locate_before = counters("A")
        send("B", "clear-endpoint 0")
        send("B", "corrupt-next-tx-mac")
        send("B", "read 50001")
        drain(0.1)
        bad_locate_after = counters("A")
        require(bad_locate_after["invalid_locate"] ==
                bad_locate_before["invalid_locate"] + 1 and
                bad_locate_after["locate_reply_tx"] ==
                bad_locate_before["locate_reply_tx"] and
                bad_locate_after["session_init_rx"] ==
                bad_locate_before["session_init_rx"] and
                bad_locate_after["reads"] == bad_locate_before["reads"],
                "invalid LOCATE MAC creates no reply, session, or app dispatch")
        b0 = len(logs["B"])
        send("B", "clear-endpoint 0")
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "valid LOCATE recovers after rejected LOCATE")

        bad_session_before = counters("B")
        bad_session_reads_before = counters("A")["reads"]
        state_events_before = event_count("B", "STATE resource=50001")
        b0 = len(logs["B"])
        send("B", "forget-session 0")
        send("B", "pause-session-init-retries")
        require(drain(2, lambda: seen("B", "OK session forgotten", b0) and
                      seen("B", "OK session-init retries paused", b0)),
                "pause SESSION_INIT retries for deterministic fault check")
        a0 = len(logs["A"])
        send("A", "corrupt-next-session-mac-tx")
        require(drain(2, lambda: seen(
                    "A", "OK hook=corrupt-next-session-mac-tx", a0)),
                "arm SESSION_ACCEPT MAC corruption")
        b0 = len(logs["B"])
        send("B", "read 50001")
        drain(0.1)
        bad_session_rejected = counters("B")
        reads_during_rejection = counters("A")["reads"]
        require(bad_session_rejected["invalid_session"] ==
                bad_session_before["invalid_session"] + 1 and
                bad_session_rejected["session_established"] ==
                bad_session_before["session_established"] and
                reads_during_rejection == bad_session_reads_before and
                event_count("B", "STATE resource=50001") == state_events_before,
                "tampered SESSION_ACCEPT creates no session or app dispatch")
        send("B", "resume-session-init-retries")
        require(drain(2, lambda: seen(
                    "B", "OK session-init retries resumed", b0)),
                "resume SESSION_INIT retries after rejection check")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "valid SESSION retry recovers after tampered SESSION_ACCEPT")
        bad_session_after = counters("B")
        require(bad_session_after["invalid_session"] >
                bad_session_before["invalid_session"],
                "invalid SESSION authentication is rejected")

        auth_before_snapshot = counters("A")
        auth_before = auth_before_snapshot["auth_fail"]
        controls_before = event_count("A", "CONTROL resource=50001")
        send("B", "corrupt-next-tx-tag")
        send("B", "control 50001 0")
        drain(0.5)
        auth_after_snapshot = counters("A")
        auth_after = auth_after_snapshot["auth_fail"]
        require(event_count("A", "CONTROL resource=50001") == controls_before and
                auth_after > auth_before and
                auth_after_snapshot["ack_tx"] == auth_before_snapshot["ack_tx"],
                "tampered DATA is rejected before application dispatch")

        send("B", "capture-next-data")
        replay_control_before = event_count("A", "CONTROL resource=50001")
        control_and_ack(0)
        require(event_count("A", "CONTROL resource=50001") ==
                replay_control_before + 1,
                "capture a successfully accepted CONTROL frame")
        for index in range(65):
            control_and_ack(index & 1)
        replay_drop_before = counters("A")["replay_drop"]
        replay_control_before = event_count("A", "CONTROL resource=50001")
        send("B", "replay-captured-data")
        drain(0.15)
        replay_after = counters("A")
        require(seen("B", "OK captured DATA replayed") and
                replay_after["replay_drop"] == replay_drop_before + 1 and
                event_count("A", "CONTROL resource=50001") ==
                replay_control_before,
                "stale protected sequence outside the replay window is rejected")

        a0 = len(logs["A"])
        send("A", "read 50002")
        require(drain(5, lambda: seen("B", "READ resource=50002", b0)),
                "ACTION-only READ establishes future event interest")
        action_a_before = counters("A")
        action_b_before = counters("B")
        send("B", "emit-action 50002 4660")
        require(drain(4, lambda: seen("A", "ACTION resource=50002 action=4660",
                                      a0)), "Action Trigger delivered best effort")
        action_a_after = counters("A")
        action_b_after = counters("B")
        require(action_a_after["ack_tx"] == action_a_before["ack_tx"] and
                action_b_after["retry_tx"] == action_b_before["retry_tx"] and
                action_b_after["actions_tx"] == action_b_before["actions_tx"] + 1,
                "Action Trigger is best effort without ACK or retry")
        actions_before = len([line for line in logs["A"]
                              if "ACTION resource=50002" in line])
        send("A", "drop-next-rx DATA")
        send("B", "emit-action 50002 22136")
        drain(1)
        send("A", "read 50002")
        drain(2)
        require(len([line for line in logs["A"]
                     if "ACTION resource=50002" in line]) == actions_before,
                "missed Action Trigger is never replayed")

        b0 = len([line for line in logs["B"] if "payload_bytes=206" in line])
        send("A", "force-max-datagram 96")
        send("A", "send-extended 50001 200")
        require(drain(4, lambda: len([line for line in logs["B"]
                                      if "payload_bytes=206" in line]) > b0),
                "forced fragmentation reassembles one complete event")
        partial_state_count = len([line for line in logs["B"]
                                   if "payload_bytes=206" in line])
        send("A", "drop-fragment-number 3")
        send("A", "send-extended 50001 200")
        drain(0.2)
        partial_deadline = time.monotonic() + 0.6
        partial_seen = False
        while time.monotonic() < partial_deadline and not partial_seen:
            b0 = len(logs["B"])
            send("B", "show-pools")
            partial_seen = drain(0.08, lambda: seen(
                "B", "reassembly=1/1", b0))
        require(partial_seen and len([line for line in logs["B"]
                                      if "payload_bytes=206" in line]) ==
                partial_state_count,
                "partial fragmented message occupies only one slot")
        time.sleep(1.1)
        send("B", "show-pools")
        require(drain(2, lambda: seen("B", "reassembly=0/1")) and
                len([line for line in logs["B"]
                     if "payload_bytes=206" in line]) == partial_state_count,
                "incomplete reassembly expires and releases its slot")

        oversized_before = counters("A")["reassembly_rejected"]
        send("B", "inject-oversized-fragment 0")
        drain(0.15)
        oversized_after = counters("A")["reassembly_rejected"]
        require(seen("B", "OK oversized fragment injected") and
                oversized_after == oversized_before + 1 and
                pools("A")["reassembly"][0] == 0,
                "oversized fragment is rejected without allocating reassembly")
        send("A", "force-max-datagram 250")
        drain(0.1)

        flood_baseline = counters("A")
        send("B", "flood-invalid-locate 1000")
        require(drain(2, lambda: seen("B", "OK flood started")),
                "invalid LOCATE flood starts")
        drain(5)
        locate_flood = counters("A")
        require(locate_flood["invalid_locate"] >=
                flood_baseline["invalid_locate"] + 1000,
                "1000 invalid LOCATE packets are rejected")
        send("B", "flood-invalid-session 1000")
        require(drain(2, lambda: seen("B", "OK flood started")),
                "invalid SESSION flood starts")
        drain(5)
        session_flood = counters("A")
        require(session_flood["invalid_session"] >=
                locate_flood["invalid_session"] + 1000,
                "1000 invalid SESSION packets are rejected")
        send("B", "flood-invalid-data 1000")
        require(drain(2, lambda: seen("B", "OK flood started")),
                "invalid DATA flood starts")
        drain(5)
        data_flood = counters("A")
        require(data_flood["invalid_data"] >=
                session_flood["invalid_data"] + 1000,
                "1000 invalid DATA packets are rejected")
        flood_pools_a = pools("A")
        flood_pools_b = pools("B")
        require(all(values[0] <= values[1] and values[2] <= values[1]
                    for pool in (flood_pools_a, flood_pools_b)
                    for name, values in pool.items()
                    if name != "workspace_bytes"),
                "all bounded pools stay within configured maxima after floods")

        send("A", "reset-test-counters")
        send("B", "reset-test-counters")
        drain(0.1)
        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "post-fault READ works without reboot")
        control_and_ack(1)
        control_and_ack(0)
        a0 = len(logs["A"])
        send("A", "read 50002")
        require(drain(4, lambda: seen("B", "READ resource=50002", a0)),
                "post-fault READ refreshes Action Trigger interest")
        action_before = event_count("A", "ACTION resource=50002")
        send("B", "emit-action 50002 4660")
        require(drain(4, lambda: event_count("A", "ACTION resource=50002") ==
                      action_before + 1),
                "post-fault Action Trigger is delivered")
        send("A", "show-status")
        send("B", "show-status")
        drain(0.2)
        final_pools_a = pools("A")
        final_pools_b = pools("B")
        require(flood_pools_a["workspace_bytes"] ==
                final_pools_a["workspace_bytes"] and
                flood_pools_b["workspace_bytes"] ==
                final_pools_b["workspace_bytes"],
                "fixed runtime workspace stays unchanged after fault tests")
        require(seen("A", "COUNTERS ") and seen("B", "POOLS"),
                "final counters and pool/resource status are available")
        print("PASS: final post-fault READ, CONTROL ON/OFF and Action Trigger",
              flush=True)
        print("LINUX_LINUX_ACCEPTANCE=PASS", flush=True)
    except Exception as error:
        print(str(error), file=sys.stderr, flush=True)
        for role in ("A", "B"):
            if role in processes and processes[role].poll() is None:
                send(role, "show-status")
        drain(0.5)
        print("LINUX_LINUX_ACCEPTANCE=FAIL", flush=True)
        return 1
    finally:
        for process in processes.values():
            if process.poll() is None:
                try:
                    process.stdin.write(b"quit\n")
                    process.stdin.flush()
                except BrokenPipeError:
                    pass
        for process in processes.values():
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=2)
        config_dir.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
