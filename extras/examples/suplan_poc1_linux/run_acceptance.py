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


def check_data_comparison(role, logs, send, drain):
    """Require the response to this query, never a PASS from an older capture."""
    after = len(logs[role])

    def response():
        return next((line for line in logs[role][after:]
                     if line.startswith("DATA_COMPARE=")), None)

    send(role, "show-data-comparison")
    drain(1, lambda: response() is not None)
    return response() == "DATA_COMPARE=PASS"


def check_data_retry_timing(role, logs, send, drain):
    """Read peer-measured retry spacing, independent of runner scheduling."""
    after = len(logs[role])

    def response():
        return next((line for line in logs[role][after:]
                     if line.startswith("DATA_RETRY_TIMING")), None)

    send(role, "show-data-retry-timing")
    drain(1, lambda: response() is not None)
    match = re.fullmatch(r"DATA_RETRY_TIMING delay_ms=(\d+)", response() or "")
    return match is not None and int(match.group(1)) >= 150


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--bind-a", default="0.0.0.0")
    parser.add_argument("--bind-b", default="0.0.0.0")
    parser.add_argument("--port-a", default="2017")
    parser.add_argument("--port-b", default="2018")
    parser.add_argument("--discovery-audit", action="store_true",
                        help="also characterize absent/late peer and self-test")
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

    def stop(role):
        process = processes.pop(role)
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        try:
            selector.unregister(process.stdout)
        except KeyError:
            pass
        process.stdin.close()
        process.stdout.close()
        partial[role] = b""

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

    def wait_for_counter(role, name, minimum, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            before = len([line for line in logs[role]
                          if line.startswith("COUNTERS ")])
            send(role, "show-counters")
            if not drain(min(1.0, deadline - time.monotonic()), lambda:
                         len([line for line in logs[role]
                              if line.startswith("COUNTERS ")]) > before):
                continue
            rows = [line for line in logs[role]
                    if line.startswith("COUNTERS ")]
            values = {key: int(value) for key, value in
                      re.findall(r"(\w+)=(\d+)", rows[-1])}
            if values.get(name, 0) >= minimum:
                return values
            time.sleep(min(0.05, max(0.0, deadline - time.monotonic())))
        return None

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

    def peer_endpoint(role, index, after=0):
        pattern = re.compile(
            rf"PEER index={index} endpoint_state=(\d+) endpoint=([^ ]+)")
        for line in reversed(logs[role][after:]):
            match = pattern.search(line)
            if match is not None:
                return int(match.group(1)), match.group(2)
        return None

    def multicast_interface_counts(role):
        rows = [line for line in logs[role]
                if line.startswith("Multicast interfaces ")]
        if not rows:
            raise RuntimeError(f"FAIL: {role} multicast interface status missing")
        match = re.search(r"(\d+) available, (\d+) joined", rows[-1])
        if match is None:
            raise RuntimeError(f"FAIL: {role} multicast interface status invalid")
        return int(match.group(1)), int(match.group(2))

    def multicast_send_counts(role):
        rows = [line for line in logs[role]
                if line.startswith("Multicast interface sends ")]
        if not rows:
            raise RuntimeError(f"FAIL: {role} multicast send status missing")
        match = re.search(r"(\d+)/(\d+)$", rows[-1])
        if match is None:
            raise RuntimeError(f"FAIL: {role} multicast send status invalid")
        return int(match.group(1)), int(match.group(2))

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

    def require_primary_endpoint(role, expected_port, description):
        before = len(logs[role])
        send(role, "show-status")
        require(drain(5, lambda: seen(
                    role, "PEER index=0 endpoint_state=", before)),
                f"{role} reports its authenticated endpoint")
        endpoint = peer_endpoint(role, 0, before)
        require(endpoint is not None and endpoint[0] == 2 and
                endpoint[1].endswith(f":{expected_port}"), description)

    try:
        start("B", args.bind_b, config_b)
        if args.discovery_audit:
            require(drain(3, lambda: seen("B", "READY role=B")),
                    "requester starts without responder process")
            send("B", "multicast-self-test")
            require(drain(4, lambda: seen(
                "B", "Local multicast receive  PASS") and seen(
                "B", "Unicast communication    NOT OBSERVED")),
                "self-test passes without any responding SupLAN peer")
            before = counters("B")
            send("B", "read 50001")
            drain(1.1)
            absent = counters("B")
            require(absent["locate_tx"] >= before["locate_tx"] + 3 and
                    absent["locate_reply_rx"] == before["locate_reply_rx"],
                    "absent peer produces repeated LOCATE attempts")
            require(pools("B")["deferredEvents"][0] == 0,
                    "ADR-008 releases foreground READ buffer while peer is absent")
        start("A", args.bind_a, config_a, args.port_a)
        if args.discovery_audit:
            require(drain(10, lambda: seen("B", "STATE resource=50001")),
                    "late responder services a fresh READ after bounded recovery")
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
        for role in ("A", "B"):
            available, joined = multicast_interface_counts(role)
            sent, attempted = multicast_send_counts(role)
            require(available > 0 and joined == available and
                    attempted == available and sent == attempted,
                    f"{role} sends and joins multicast on every active interface")

        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0) and
                      seen("B", "ACK resource=50001", b0)),
                "authenticated LOCATE, SESSION and acknowledged current-state READ")
        b0 = len(logs["B"])
        send("B", "show-status")
        require(drain(8, lambda: seen(
                    "B", "PEER index=0 endpoint_state=", b0)),
                "B reports its authenticated primary endpoint")
        endpoint_b = peer_endpoint("B", 0, b0)
        expected_b_address = (args.bind_a if args.bind_a != "0.0.0.0"
                              else None)
        endpoint_match = (
            endpoint_b is not None and endpoint_b[0] == 2 and
            endpoint_b[1].endswith(f":{args.port_a}") and
            (expected_b_address is None or
             endpoint_b[1] == f"{expected_b_address}:{args.port_a}"))
        if not endpoint_match:
            print(f"FAIL: authenticated peer endpoints do not match this run "
                  f"(B={endpoint_b}; expected A at port {args.port_a})",
                  flush=True)
        require(endpoint_match,
                "LOCATE selected this run's intended A test process")

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
        require(check_data_comparison("B", logs, send, drain),
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

        sessions_before_reboot = counters("B")["session_established"]
        retries_before_reboot = counters("B")["retry_tx"]
        locates_before_reboot = counters("B")["locate_tx"]
        states_before_reboot = event_count("B", "STATE resource=50001")
        acks_before_reboot = event_count("B", "ACK resource=50001")
        a0 = len(logs["A"])
        stop("A")
        start("A", args.bind_a, config_a, args.port_a)
        require(drain(3, lambda: seen("A", "READY role=A", a0)),
                "responder process restarts with its provisioned peer")
        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(6, lambda: seen("B", "STATE resource=50001", b0) and
                      seen("B", "ACK resource=50001", b0) and
                      event_count("B", "STATE resource=50001") >
                      states_before_reboot and
                      event_count("B", "ACK resource=50001") >
                      acks_before_reboot),
                "READ recovers automatically after peer reboot")
        after_reboot = counters("B")
        require(after_reboot["retry_tx"] > retries_before_reboot and
                after_reboot["session_established"] > sessions_before_reboot and
                after_reboot["locate_tx"] == locates_before_reboot,
                "peer reboot recovery retries stale READ then handshakes at retained endpoint")

        bad_locate_before = counters("A")
        send("B", "clear-endpoint 0")
        send("B", "corrupt-next-tx-mac")
        send("B", "read 50001")
        drain(0.1)
        bad_locate_after = counters("A")
        # Auto interface selection sends one LOCATE on each eligible
        # interface, so a local peer can observe multiple invalid copies.
        require(bad_locate_after["invalid_locate"] >
                bad_locate_before["invalid_locate"] and
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
        # Let every copy from all selected multicast interfaces and any
        # duplicate SESSION traffic leave the bounded recovery window before
        # injecting the next handshake fault.
        drain(1.0)
        settled_pools_a = pools("A")
        settled_pools_b = pools("B")
        require(settled_pools_a["pending"][0] == 0 and
                settled_pools_a["locates"][0] == 0 and
                settled_pools_b["pending"][0] == 0 and
                settled_pools_b["locates"][0] == 0,
                "no deferred LOCATE or SESSION recovery remains before fault injection")

        # Isolate handshake authentication from recovery and endpoint churn.
        # A fresh pair has one matching active session before B deliberately
        # forgets its side and sends a tampered SESSION_INIT.
        a0 = len(logs["A"])
        b0 = len(logs["B"])
        stop("A")
        stop("B")
        start("A", args.bind_a, config_a, args.port_a)
        start("B", args.bind_b, config_b)
        require(drain(3, lambda: seen("A", "READY role=A", a0) and
                      seen("B", "READY role=B", b0)),
                "restart both peers before SESSION authentication test")
        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "fresh SESSION established before authentication fault")
        require_primary_endpoint(
            "B", args.port_a,
            "SESSION authentication test targets this run's A process")
        bad_session_before_a = counters("A")
        bad_session_before_b = counters("B")
        state_events_before = event_count("B", "STATE resource=50001")
        b0 = len(logs["B"])
        send("B", "forget-session 0")
        send("B", "pause-session-init-retries")
        require(drain(2, lambda: seen("B", "OK session forgotten", b0) and
                      seen("B", "OK session-init retries paused", b0)),
                "pause SESSION_INIT retries for deterministic fault check")
        b0 = len(logs["B"])
        send("B", "corrupt-next-session-mac-tx")
        require(drain(2, lambda: seen(
                    "B", "OK hook=corrupt-next-session-mac-tx", b0)),
                "arm SESSION_INIT MAC corruption")
        b0 = len(logs["B"])
        send("B", "read 50001")
        bad_session_rejected_a = wait_for_counter(
            "A", "invalid_session",
            bad_session_before_a["invalid_session"] + 1, 3)
        require(bad_session_rejected_a is not None,
                "A rejects the tampered SESSION_INIT MAC")
        bad_session_rejected_b = counters("B")
        require(bad_session_rejected_a["session_init_rx"] ==
                bad_session_before_a["session_init_rx"] and
                bad_session_rejected_a["session_accept_tx"] ==
                bad_session_before_a["session_accept_tx"] and
                bad_session_rejected_a["reads"] == bad_session_before_a["reads"] and
                bad_session_rejected_b["session_established"] ==
                bad_session_before_b["session_established"] and
                event_count("B", "STATE resource=50001") == state_events_before,
                "tampered SESSION_INIT creates no session or app dispatch")
        send("B", "resume-session-init-retries")
        require(drain(2, lambda: seen(
                    "B", "OK session-init retries resumed", b0)),
                "resume SESSION_INIT retries after rejection check")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "valid SESSION retry recovers after tampered SESSION_INIT")
        bad_session_after = counters("A")
        require(bad_session_after["invalid_session"] >
                bad_session_before_a["invalid_session"],
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

        # The preceding recovery/fault cases deliberately leave sessions and
        # pending network work in motion. Start both fixture processes fresh
        # so the Action Trigger profile begins with one known READ interest
        # and no unrelated pending handshake traffic.
        a0 = len(logs["A"])
        b0 = len(logs["B"])
        stop("A")
        stop("B")
        start("A", args.bind_a, config_a, args.port_a)
        start("B", args.bind_b, config_b)
        require(drain(3, lambda: seen("A", "READY role=A", a0) and
                      seen("B", "READY role=B", b0)),
                "restart both peers before isolated Action Trigger tests")
        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0)),
                "fresh sessions are established for both PoC1 resources")

        a0 = len(logs["A"])
        b0 = len(logs["B"])
        send("A", "read 50002")
        require(drain(5, lambda: seen("B", "READ resource=50002", b0) and
                      seen("A", "ACK resource=50002", a0)),
                "ACTION-only READ establishes future event interest")
        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        action_a_before = counters("A")
        action_b_before = counters("B")
        send("B", "emit-action 50002 4660")
        require(drain(4, lambda: event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "Action Trigger is acknowledged exactly once")
        action_a_after = counters("A")
        action_b_after = counters("B")
        require(action_a_after["ack_tx"] == action_a_before["ack_tx"] + 1 and
                action_b_after["ack_rx"] == action_b_before["ack_rx"] + 1 and
                action_b_after["actions_tx"] == action_b_before["actions_tx"] + 1,
                "Action Trigger sets ACK_REQUIRED and releases retry state")
        require(pools("B")["retries"][0] == 0,
                "acknowledged Action Trigger releases its retry slot")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        action_a_before = counters("A")
        action_b_before = counters("B")
        send("B", "capture-next-data compare-retry")
        drain(0.05)
        send("A", "drop-next-rx DATA")
        drain(0.05)
        send("B", "emit-action 50002 22136")
        require(drain(3, lambda:
                      event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "dropped Action Trigger DATA is recovered by one retry")
        require(check_data_retry_timing("B", logs, send, drain),
                "dropped DATA retry uses identical frame after at least 150 ms")
        action_a_after = counters("A")
        action_b_after = counters("B")
        require(action_b_after["retry_tx"] == action_b_before["retry_tx"] + 1 and
                action_a_after["actions_rx"] == action_a_before["actions_rx"] + 1,
                "recovered Action Trigger dispatches once and completes ACK")
        require(check_data_comparison("B", logs, send, drain),
                "Action Trigger DATA retry is byte-identical")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        action_a_before = counters("A")
        action_b_before = counters("B")
        send("B", "capture-next-data compare-retry")
        drain(0.05)
        send("A", "drop-next-tx ACK")
        drain(0.05)
        send("B", "emit-action 50002 22137")
        require(drain(3, lambda:
                      event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "lost Action Trigger ACK is recovered by duplicate re-ACK")
        require(check_data_comparison("B", logs, send, drain),
                "lost-ACK Action Trigger retry is byte-identical")
        action_a_after = counters("A")
        action_b_after = counters("B")
        require(action_a_after["actions_rx"] == action_a_before["actions_rx"] + 1 and
                action_b_after["retry_tx"] == action_b_before["retry_tx"] + 1 and
                action_a_after["duplicate"] == action_a_before["duplicate"] + 1,
                "duplicate Action Trigger is ACKed without redispatch")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        action_b_before = counters("B")
        action_a_before = counters("A")
        send("B", "capture-next-data compare-retry")
        drain(0.05)
        send("A", "drop-next-tx ACK 3")
        drain(0.05)
        send("B", "emit-action 50002 22138")
        drain(0.7)
        action_b_after = counters("B")
        action_a_after = counters("A")
        require(event_count("A", "ACTION resource=50002") ==
                action_events_before + 1 and
                event_count("B", "ACK resource=50002") == action_acks_before and
                action_b_after["retry_tx"] == action_b_before["retry_tx"] + 2 and
                action_b_after["data_tx"] == action_b_before["data_tx"] + 3 and
                action_a_after["actions_rx"] == action_a_before["actions_rx"] + 1 and
                action_a_after["duplicate"] == action_a_before["duplicate"] + 2 and
                pools("B")["retries"][0] == 0,
                "Action Trigger stops after three DATA attempts without ACK")
        require(check_data_comparison("B", logs, send, drain),
                "bounded Action Trigger retries use the identical frame")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        action_b_before = counters("B")
        action_a_before = counters("A")
        send("B", "fail-next-tx DATA")
        drain(0.05)
        send("B", "emit-action 50002 22139")
        require(drain(3, lambda:
                      event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "synchronous send failure retries after the bounded delay")
        require(check_data_retry_timing("B", logs, send, drain),
                "failed DATA retry uses identical frame after at least 150 ms")
        action_b_after = counters("B")
        action_a_after = counters("A")
        require(action_b_after["retry_tx"] == action_b_before["retry_tx"] + 1 and
                action_b_after["data_tx"] == action_b_before["data_tx"] + 1 and
                action_a_after["actions_rx"] == action_a_before["actions_rx"] + 1,
                "local send failure consumes an attempt and then dispatches once")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        session_init_before = counters("B")["session_init_tx"]
        # Role B maps Action Trigger resource 50002 to peer index 1.
        send("B", "forget-session 1")
        send("B", "emit-action 50002 22140")
        require(drain(4, lambda:
                      event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "Action Trigger establishes a missing SESSION directly")
        require(counters("B")["session_init_tx"] > session_init_before,
                "Action-only delivery initiates SESSION recovery")

        action_events_before = event_count("A", "ACTION resource=50002")
        action_acks_before = event_count("B", "ACK resource=50002")
        locate_before = counters("B")["locate_tx"]
        send("B", "clear-endpoint 1")
        send("B", "emit-action 50002 22141")
        require(drain(5, lambda:
                      event_count("A", "ACTION resource=50002") ==
                      action_events_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_acks_before + 1),
                "Action Trigger establishes LOCATE and SESSION without READ")
        require(counters("B")["locate_tx"] > locate_before,
                "Action-only delivery performs authenticated LOCATE")

        actions_before_unavailable = event_count(
            "A", "ACTION resource=50002")
        stop("A")
        send("B", "clear-endpoint 1")
        send("B", "emit-action 50002 22142")
        drain(3.2)
        a0 = len(logs["A"])
        start("A", args.bind_a, config_a, args.port_a)
        require(drain(3, lambda: seen("A", "READY role=A", a0)),
                "peer returns after Action Trigger expiry")
        b0 = len(logs["B"])
        acks_before_refresh = event_count("A", "ACK resource=50002")
        send("A", "read 50002")
        require(drain(5, lambda: seen("B", "READ resource=50002", b0) and
                      event_count("A", "ACK resource=50002") >
                      acks_before_refresh),
                "READ refreshes interest after peer recovery")
        require(event_count("A", "ACTION resource=50002") ==
                actions_before_unavailable,
                "expired Action Trigger is not replayed after peer recovery")

        b0 = len(logs["B"])
        send("B", "read 50001")
        require(drain(5, lambda: seen("B", "STATE resource=50001", b0) and
                      seen("B", "ACK resource=50001", b0)),
                "READ refreshes relay interest after responder restart")

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

        # A native ACK also travels inside protected DATA. It must not consume
        # the application-DATA failure hook or timing observation.
        a0 = len(logs["A"])
        send("B", "fail-next-tx DATA")
        send("A", "read 50002")
        require(drain(3, lambda: seen("A", "ACK resource=50002", a0)),
                "ACK preceding application DATA does not consume failure hook")
        control_and_ack(1)
        require(check_data_retry_timing("B", logs, send, drain),
                "application CONTROL still fails once and retries after ACK")

        # Fault injection happens before adaptation, including fragmented DATA.
        b0 = len(logs["B"])
        a0 = len(logs["A"])
        send("A", "force-max-datagram 96")
        send("A", "fail-next-tx DATA")
        send("A", "send-extended 50001 200")
        require(drain(2, lambda: seen(
                    "A", "ERROR extended state rejected", a0)),
                "fragmented DATA observes synchronous failure before send")
        drain(0.2)
        require(not seen("B", "payload_bytes=206", b0),
                "failed fragmented DATA sends no partial or complete state")
        send("A", "send-extended 50001 200")
        require(drain(3, lambda: seen("B", "payload_bytes=206", b0)),
                "fragment failure consumes hook and the next state recovers")
        send("A", "force-max-datagram 250")

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
        action_ack_before = event_count("B", "ACK resource=50002")
        send("B", "emit-action 50002 4660")
        require(drain(4, lambda: event_count("A", "ACTION resource=50002") ==
                      action_before + 1 and
                      event_count("B", "ACK resource=50002") ==
                      action_ack_before + 1),
                "post-fault Action Trigger is acknowledged")
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
