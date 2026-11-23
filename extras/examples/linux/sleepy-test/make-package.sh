#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
umask 077
if [[ $# != 1 || ! -x $1 ]]; then
    echo "Usage: $0 path/to/Release/supla-device-linux" >&2
    exit 2
fi
binary=$(realpath -- "$1")
here=$(cd -- "$(dirname -- "$0")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel)
cd -- "$repo"
work=$(mktemp -d /tmp/codex/suplan-sleepy-package.XXXXXXXX)
echo "$work"
package="$work/suplan-sleepy-test"
mkdir -p -- "$package"/{bin,config,scripts}
install -m 0755 -- "$binary" "$package/bin/supla-device-linux"
cp -- "$here"/config/*.yaml.example "$here/config/sleepy.conf.example" "$package/config/"
cp -a -- "$here"/scripts/{common,run,stop,status,smoke-test,collect-logs}.sh     "$here/scripts/verify-linux.py" "$package/scripts/"
cp -- "$here/README.md" LICENSE "$package/"
# Use an explicit source tree allowlist. Generated builds and local configs
# cannot enter the archive through an untracked file or an ignored directory.
{
    git ls-tree -rz --name-only HEAD -- LICENSE src cmake extras/porting/linux extras/examples/linux
    printf '%s\0' extras/examples/linux/sleepy_thermometer.{cpp,h}
} | sort -zu >"$work/source-files"
tar -czf "$package/suplan-sleepy-source.tar.gz" --null -T "$work/source-files"
{
    printf 'Git base: %s (%s)\n' "$(git rev-parse HEAD)" "$(git branch --show-current)"
    echo 'Source: worktree with the Sleepy Thermometer changes; complete build inputs in source archive.'
    echo 'Profile: SERVER provisioning, SUPLAN_BUILD_TEST_HARNESS=OFF, SUPLA_LINUX_ENABLE_HTTP_SOURCE=OFF.'
    (cd "$package" && bin/supla-device-linux --version && file bin/supla-device-linux)
    cat /etc/os-release
    echo
    echo 'Dynamic libraries (build host and SONAMEs):'
    ldd "$binary"
    echo
    echo 'Highest required versioned symbols:'
    for family in 'GLIBC_' 'GLIBCXX_' 'CXXABI_' 'OPENSSL_'; do
        readelf --version-info "$binary" |
            awk -v family="$family" '$2 == "Name:" && index($3, family) == 1 {print $3}' |
            sort -V | tail -n 1
    done
    echo
    echo 'SHA-256:'
    (cd "$package" && sha256sum bin/supla-device-linux suplan-sleepy-source.tar.gz)
} >"$package/BUILD_INFO.txt"
tar -C "$work" -czf "$work/suplan-sleepy-test.tar.gz" suplan-sleepy-test
