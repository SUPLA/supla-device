#!/usr/bin/env python3
# SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run current Destination against a real pre-Native/9 portable Source.

Requires a locally available legacy git revision and C++/OpenSSL toolchain.
No network, Server, fixture files in the SDK or Docker services are required.
Builds live exclusively under a unique /tmp/codex directory. This optional
cross-version harness is separate from ordinary current-version CTest.
"""
import argparse
import io
from pathlib import Path
import subprocess
import tarfile
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--legacy-ref', required=True)
    parser.add_argument('--valgrind', action='store_true')
    args = parser.parse_args()
    checkout = Path(__file__).resolve().parents[3]
    revision = subprocess.check_output(
        ['git', 'rev-parse', '--verify', args.legacy_ref + '^{commit}'],
        cwd=checkout, text=True).strip()
    work = Path(tempfile.mkdtemp(prefix='suplan-native9-compat-', dir='/tmp/codex'))
    print(f'Legacy Source: {revision}\nArtifacts: {work}', flush=True)
    archive = subprocess.check_output(
        ['git', 'archive', revision, 'src', 'extras/porting/linux'], cwd=checkout)
    legacy = work / 'legacy'
    legacy.mkdir()
    with tarfile.open(fileobj=io.BytesIO(archive)) as stream:
        stream.extractall(legacy, filter='data')
    if 'kNativeControlAction' in (legacy / 'src/suplan/suplan_data.h').read_text():
        raise SystemExit('Choose a legacy revision without Native/9 support')
    test = Path(__file__).resolve().parent / 'legacy_native9'
    portable = ['wire', 'crypto', 'session', 'acl', 'data', 'fragment', 'runtime']
    objects = []
    for tag, root in [('legacy', legacy), ('current', checkout)]:
        sources = [root / f'src/suplan/suplan_{name}.cpp' for name in portable]
        sources += [root / 'src/supla/crypto.cpp',
                    root / 'src/supla/sha256.cpp',
                    root / 'extras/porting/linux/suplan_crypto_openssl.cpp',
                    test / ('legacy_native9_peer.cpp' if tag == 'legacy'
                            else 'legacy_native9_compat.cpp')]
        for number, source in enumerate(sources):
            obj = work / f'{tag}-{number}.o'
            cmd = ['g++', '-std=c++17', '-O1', '-g',
                   '-ffunction-sections', '-fdata-sections', '-DSUPLA_DEVICE',
                   '-DSUPLA_LINUX', '-DSUPLA_DISABLE_LOGS',
                   '-I' + str(root / 'src'),
                   '-I' + str(root / 'src/supla-common'),
                   '-I' + str(root / 'extras/porting/linux'), '-I' + str(test)]
            if tag == 'legacy':
                cmd.append('-DSupla=LegacySupla')
            subprocess.run(cmd + ['-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
    executable = work / 'legacy-native9-compat'
    subprocess.run(['g++', *objects, '-Wl,--gc-sections', '-lcrypto',
                    '-o', str(executable)], check=True)
    command = [str(executable)]
    if args.valgrind:
        command = ['valgrind', '--error-exitcode=99', '--leak-check=full',
                   '--errors-for-leak-kinds=definite', *command]
    subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
