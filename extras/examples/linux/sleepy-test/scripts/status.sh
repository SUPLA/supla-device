#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
# shellcheck source=common.sh
source "$(dirname -- "$0")/common.sh"
if pid=$(supervisor_pid); then
    printf 'Supervisor PID %s\n' "$pid"
else
    echo "Supervisor is stopped"
fi
latest=
for log in "$runtime"/logs/run.*.log; do
    if [[ -f $log && ( -z $latest || $log -nt $latest ) ]]; then latest=$log; fi
done
if [[ -n $latest ]]; then
    printf 'Log %s\n' "$latest"
    awk '/^RUNNER |^SLEEPY / {print}' "$latest" | tail -12
fi
