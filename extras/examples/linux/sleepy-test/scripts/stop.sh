#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
# shellcheck source=common.sh
source "$(dirname -- "$0")/common.sh"
if pid=$(supervisor_pid); then
    kill -TERM "$pid"
    for ((attempt=0; attempt<60; ++attempt)); do
        if ! supervisor_pid >/dev/null; then exit 0; fi
        sleep 0.1
    done
    echo "Supervisor PID $pid is still stopping" >&2
    exit 1
fi
echo "Supervisor is stopped"
