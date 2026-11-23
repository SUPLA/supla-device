#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
# shellcheck source=common.sh
source "$(dirname -- "$0")/common.sh"

usage() {
    echo "Usage: $0 [normal|kill|skip|provision] [number_of_slots, 0=infinite]" >&2
    exit 2
}

mode=${1:-normal}
limit=${2:-0}
case "$mode" in normal|kill|skip|provision) ;; *) usage ;; esac
[[ $# -le 2 && $limit =~ ^(0|[1-9][0-9]{0,8})$ ]] || usage
[[ -x $binary && -r config/supla-device.yaml ]] || {
    echo "Install bin/supla-device-linux and config/supla-device.yaml first." >&2
    exit 2
}
# shellcheck source=../config/sleepy.conf.example
source config/sleepy.conf.example
if [[ -f config/sleepy.conf ]]; then
    # shellcheck source=/dev/null
    source config/sleepy.conf
fi
for key in period_ms ready_ms awake_ms tail_ms fault_slot kill_after_ms skip_slots; do
    [[ ${!key} =~ ^[1-9][0-9]{0,8}$ ]] || usage
done
(( ready_ms <= 3600000 && awake_ms <= 65535 && tail_ms <= 30000 )) || usage
if [[ $mode != provision ]]; then
    (( period_ms > ready_ms + awake_ms + tail_ms + 1500 )) || {
        echo "period_ms must exceed ready_ms + awake_ms + tail_ms + 1500." >&2
        exit 2
    }
fi
[[ $mode != kill ]] || (( kill_after_ms < awake_ms )) || usage

mkdir -p "$runtime/sdk" "$runtime/logs"
exec 9>"$runtime/supervisor.lock"
flock -n 9 || { echo "Supervisor already running" >&2; exit 1; }

logfile=$(mktemp "$runtime/logs/run.XXXXXXXX.log")
child=
stop_code=0
cycle=0
slot=0
event() {
    local now_ms
    boot_ms
    printf 'RUNNER cycle=%s slot=%s ts=%s boot_ms=%s event=%s %s\n' \
        "$cycle" "$slot" "$(date +%s.%3N)" "$now_ms" "$1" "${2:-}" >>"$logfile"
}

terminate_child() {
    [[ -n $child ]] || return 0
    kill -TERM "$child" 2>/dev/null || :
    local attempt
    for ((attempt=0; attempt<30; ++attempt)); do
        kill -0 "$child" 2>/dev/null || break
        sleep 0.1 9>&-
    done
    kill -KILL "$child" 2>/dev/null || :
    wait "$child" 2>/dev/null || :
    child=
}

cleanup() {
    if [[ -n $child ]]; then
        terminate_child
        event CHILD_STOPPED
    fi
    rm -f -- "$runtime/supervisor"
}

stop_if_requested() {
    if (( stop_code )); then exit "$stop_code"; fi
}

trap cleanup EXIT
trap 'stop_code=130' INT
trap 'stop_code=143' TERM
printf '%s %s\n' "$$" "$(proc_start "$$")" >"$runtime/supervisor"
printf '%s\n' "$logfile"
event RUN_START "mode=$mode pid=$$ slots=$limit period_ms=$period_ms fault_slot=$fault_slot skip_slots=$skip_slots"

write_runtime() {
    local temporary
    temporary=$(mktemp "$runtime/write.XXXXXXXX")
    printf '%s\n' "$2" >"$temporary"
    mv -f -- "$temporary" "$runtime/$1"
}

if [[ $mode == provision ]]; then
    write_runtime temperature.txt 20.0
    "$binary" -c config/supla-device.yaml >>"$logfile" 2>&1 9>&- &
    child=$!
    stop_if_requested
    event APP_STARTED "pid=$child mode=provision"
    while kill -0 "$child" 2>/dev/null; do
        stop_if_requested
        sleep 0.1 9>&-
    done
    rc=0
    wait "$child" || rc=$?
    stop_if_requested
    child=
    event PROVISION_EXIT "code=$rc"
    exit "$rc"
fi

if [[ -e $runtime/cycle ]]; then
    read -r cycle <"$runtime/cycle"
    [[ $cycle =~ ^(0|[1-9][0-9]{0,15})$ ]] || {
        echo "Invalid cycle file" >&2
        exit 1
    }
fi

temperatures=(20.0 20.5 21.0 21.5)
boot_ms
planned=$now_ms
while (( limit == 0 || slot < limit )); do
    stop_if_requested
    boot_ms
    while (( now_ms < planned )); do
        wait_ms=$((planned - now_ms))
        if (( wait_ms > 100 )); then wait_ms=100; fi
        printf -v interval '%d.%03d' "$((wait_ms / 1000))" "$((wait_ms % 1000))"
        sleep "$interval" 9>&-
        stop_if_requested
        boot_ms
    done
    ((slot+=1))
    next=$((planned + period_ms))
    boot_ms
    if (( now_ms >= next )); then
        event SLOT_SKIPPED "reason=overrun planned_ms=$planned"
    elif [[ $mode == skip ]] &&
         (( slot >= fault_slot && slot - fault_slot < skip_slots )); then
        event SLOT_SKIPPED "reason=injected planned_ms=$planned"
    else
        ((cycle+=1))
        write_runtime cycle "$cycle"
        write_runtime temperature.txt "${temperatures[(cycle - 1) % 4]}"
        stop_if_requested
        boot_ms
        launch_before_ms=$now_ms
        "$binary" -c config/supla-device.yaml --sleepy-cycle "$cycle" \
            --sleepy-ready-ms "$ready_ms" --sleepy-awake-ms "$awake_ms" \
            --sleepy-tail-ms "$tail_ms" --sleepy-next-ms "$next" \
            >>"$logfile" 2>&1 9>&- &
        child=$!
        stop_if_requested
        event APP_STARTED "pid=$child planned_ms=$planned launch_before_ms=$launch_before_ms"
        boot_ms
        launched=$now_ms
        outcome=normal
        while kill -0 "$child" 2>/dev/null; do
            stop_if_requested
            boot_ms
            if [[ $mode == kill ]] &&
               (( slot == fault_slot && now_ms - launched >= kill_after_ms )); then
                if kill -KILL "$child" 2>/dev/null; then outcome="kill"; fi
                break
            fi
            if (( now_ms - launched >= ready_ms + awake_ms + tail_ms + 3000 )); then
                outcome=watchdog
                terminate_child
                break
            fi
            sleep 0.05 9>&-
        done
        rc=124
        if [[ -n $child ]]; then
            rc=0
            wait "$child" 2>/dev/null || rc=$?
            stop_if_requested
            child=
        fi
        event CYCLE_RESULT "code=$rc outcome=$outcome"
    fi
    planned=$next
done
event RUN_END
