#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# shellcheck disable=SC2034

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root" || exit 1
umask 077
runtime=runtime
binary=bin/supla-device-linux

boot_ms() {
    local uptime whole fraction
    read -r uptime _ </proc/uptime
    whole=${uptime%.*}
    fraction=${uptime#*.}
    now_ms=$((whole * 1000 + 10#${fraction:0:2} * 10))
}

proc_start() {
    local stat fields
    IFS= read -r stat <"/proc/$1/stat" || return 1
    read -r -a fields <<< "${stat##*) }"
    printf '%s\n' "${fields[19]}"
}

supervisor_pid() {
    local pid started actual
    [[ -f $runtime/supervisor ]] || return 1
    read -r pid started <"$runtime/supervisor" || return 1
    [[ $pid =~ ^[1-9][0-9]*$ ]] || return 1
    actual=$(proc_start "$pid" 2>/dev/null) || return 1
    [[ $actual == "$started" ]] || return 1
    printf '%s\n' "$pid"
}
