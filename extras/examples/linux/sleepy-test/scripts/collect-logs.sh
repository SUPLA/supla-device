#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
# shellcheck source=common.sh
source "$(dirname -- "$0")/common.sh"
if [[ $# != 1 || $1 != /* ]]; then
    echo "Usage: $0 /absolute/path/to/new-log-directory" >&2
    exit 2
fi
destination=$1
shopt -s nullglob
logs=("$runtime"/logs/run.*.log)
(( ${#logs[@]} )) || { echo "No cycle logs" >&2; exit 1; }
mkdir -- "$destination"
cp -- "${logs[@]}" "$destination/"
awk '
function field(key,    i, parts) {
    for (i=2; i<=NF; i++) {
        split($i, parts, "=")
        if (parts[1] == key) return parts[2]
    }
    return ""
}
$1 == "SLEEPY" || $1 == "RUNNER" {
    key = FILENAME SUBSEP field("cycle")
    event = field("event")
    elapsed = field("elapsed_ms") + 0
    if (event == "APP_STARTED" && field("mode") != "provision") total++
    if (event == "SLOT_SKIPPED") skipped++
    if (event == "SYNC_FAILED" && !failed[key]++) failures++
    # Older logs only confirmed the initial datagrams.
    if (event == "ANNOUNCE_SLEEP" && field("boot_ms") == "" &&
        field("initial_datagrams") + 0 > 0) sleep_sent[key] = 1
    if (event == "SLEEP_RESULT") {
        sleep_sent[key] = field("sent") == "1"
        if (!sleep_sent[key]) sleep_failures++
    }
    if (event == "UDP_READY") {
        udp[key] = elapsed
        udp_sum += elapsed
        udp_samples++
    }
    if (event == "STATE_SENT" && !state[key]++) {
        if (key in udp) {
            send_sum += elapsed - udp[key]
            send_samples++
        }
    }
    if (event == "CYCLE_RESULT") {
        code = field("code") + 0
        if (code == 0 && state[key] && sleep_sent[key]) success++
        if (code != 0 && code != 3) abnormal++
    }
}
END {
    printf "cycles_started=%d\nsuccessful_source_cycles=%d\nsync_failures=%d\n", total, success, failures
    printf "abnormal_restarts=%d\nskipped_slots=%d\n", abnormal, skipped
    printf "sleep_announcement_failures=%d\n", sleep_failures
    if (udp_samples) printf "start_to_udp_ms_avg=%.1f samples=%d\n", udp_sum/udp_samples, udp_samples
    else print "start_to_udp_ms_avg=NA"
    if (send_samples) printf "udp_to_first_state_ms_avg=%.1f samples=%d\n", send_sum/send_samples, send_samples
    else print "udp_to_first_state_ms_avg=NA"
}' "$destination"/run.*.log | tee "$destination/summary.txt"
