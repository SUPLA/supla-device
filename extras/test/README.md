<!--
SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Host tests

Run commands from the repository root. By default, CTest registers
`supladevicetests`, `sd4linuxtests` and `arduinoesp8266_tls_lifetime_tests`.
These tests do not require network access at runtime. The HTTP source unit tests
use fake transports or local `file://` inputs; MQTT uses a simulated transport.

## Build with existing dependencies, without network access

`FetchContent` normally puts downloaded dependencies in each build directory's
`_deps` folder. A new build directory does not automatically reuse earlier
downloads. Explicit source overrides reuse those checkouts while keeping all
generated files in the new build directory.

For dependencies already present in `extras/test/build/_deps`, at the versions
declared in `CMakeLists.txt`:

```bash
sd_test_build=$(mktemp -d /tmp/codex/supla-host-tests-XXXXXX)
CCACHE_DISABLE=1 cmake -S extras/test -B "$sd_test_build" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON="$PWD/extras/test/build/_deps/json-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/extras/test/build/_deps/googletest-src" \
  -DFETCHCONTENT_SOURCE_DIR_CJSON="$PWD/extras/test/build/_deps/cjson-src" \
  -DFETCHCONTENT_SOURCE_DIR_MQTTC="$PWD/extras/test/build/_deps/mqttc-src"
CCACHE_DISABLE=1 cmake --build "$sd_test_build" -j 4
TMPDIR=/tmp/codex ctest --test-dir "$sd_test_build" --output-on-failure
```

Adjust the source paths if the existing dependency checkouts are elsewhere.
Both offline settings and the source overrides persist in this build's CMake
cache. No dependency downloads or updates are needed on subsequent builds.

## Optional HTTP integration tests

The six redirect tests exercise the real libcurl transport using local TCP
servers. They verify custom-header protection, redirect following, the five-hop
limit and rejection of FTP redirect targets. They require loopback networking
and are excluded from the default build and CTest registration.

To enable them explicitly in an already configured build:

```bash
CCACHE_DISABLE=1 cmake -S extras/test -B "$sd_test_build" \
  -DSUPLA_TEST_ENABLE_HTTP_INTEGRATION=ON
CCACHE_DISABLE=1 cmake --build "$sd_test_build" \
  --target sd4linux_http_integration_tests -j 4
TMPDIR=/tmp/codex ctest --test-dir "$sd_test_build" \
  -R '^sd4linux_http_integration_tests$' --output-on-failure
```

This requires CURL and `SUPLA_TEST_ENABLE_CURL_HTTP=ON` (the default). Once
enabled, the integration target is also part of a full build and CTest run.
Use `ctest --test-dir "$sd_test_build" -LE network --output-on-failure` to run
only tests without networking, or configure
`-DSUPLA_TEST_ENABLE_HTTP_INTEGRATION=OFF` to remove the integration target again.
