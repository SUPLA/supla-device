#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Independent process/scheduler test against an unprovisioned SERVER profile.
set -euo pipefail
# shellcheck source=common.sh
source "$(dirname -- "$0")/common.sh"
mkdir -p /tmp/codex
sandbox=$(mktemp -d /tmp/codex/suplan-sleepy-smoke.XXXXXXXX)
echo "Smoke test logs: $sandbox"
mkdir -- "$sandbox/bin"
cp -a -- config scripts "$sandbox/"
cp -- "$binary" "$sandbox/bin/"
cd "$sandbox"
sed -e 's/SERVER_HOSTNAME/127.0.0.1/' -e 's/ACCOUNT_EMAIL/sleepy-smoke@example.invalid/' \
    -e 's/unicast_port: 2016/unicast_port: 32176/' \
    config/supla-device.yaml.example >config/supla-device.yaml
cat >config/sleepy.conf <<'CONFIG'
period_ms=3400
ready_ms=900
awake_ms=700
tail_ms=200
fault_slot=2
kill_after_ms=300
skip_slots=2
CONFIG
# Test a normal timeout before and after the killed cycle and intentional skips.
kill_log=$(scripts/run.sh kill 3)
skip_log=$(scripts/run.sh skip 4)
[[ $(< runtime/cycle) == 5 ]]
[[ $(grep -c 'event=SYNC_FAILED' "$kill_log") == 2 ]]
[[ $(grep -c 'event=SLOT_SKIPPED reason=injected' "$skip_log") == 2 ]]
grep -q 'cycle=2 slot=2 .*event=CYCLE_RESULT code=137 outcome=kill' "$kill_log"
if grep -q 'cycle=2 .*event=ANNOUNCE_SLEEP' "$kill_log"; then exit 1; fi
for entry in '1 20.0' '2 20.5' '3 21.0' '4 21.5' '5 20.0'; do
    read -r cycle temperature <<< "$entry"
    grep -F 'event=TEMPERATURE_UPDATED celsius='"$temperature" "$kill_log" "$skip_log" |
        grep -q "cycle=$cycle "
done
while read -r pid; do
    if kill -0 "$pid" 2>/dev/null; then exit 1; fi
done < <(awk '/event=APP_STARTED/ {for(i=1;i<=NF;i++) if($i ~ /^pid=/) print substr($i,5)}' "$kill_log" "$skip_log")
identity=(runtime/sdk/guid_auth.yaml)
sha256sum "${identity[@]}" >identity.sha256
scripts/run.sh provision >provision.log &
supervisor=$!
trap 'scripts/stop.sh >/dev/null || :' EXIT
for ((attempt=0; attempt<50; ++attempt)); do
    [[ -s provision.log ]] && grep -q 'event=APP_STARTED' "$(cat provision.log)" && break
    sleep 0.05
done
prov_log=$(cat provision.log)
prov_pid=$(sed -n 's/.*event=APP_STARTED pid=\([0-9]*\).*/\1/p' "$prov_log")
[[ -n $prov_pid ]]
kill -0 "$prov_pid"
scripts/stop.sh
rc=0
wait "$supervisor" || rc=$?
[[ $rc == 143 ]]
if kill -0 "$prov_pid" 2>/dev/null; then exit 1; fi
sha256sum --check identity.sha256
scripts/collect-logs.sh "$sandbox/collected"
echo "PASS: Linux temperature sequence, bounded timeout, SIGKILL, skip, stop and stable identity"
