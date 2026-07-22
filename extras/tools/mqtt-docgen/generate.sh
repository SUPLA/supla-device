#!/usr/bin/env bash

set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
test_build_dir="${MQTT_TEST_BUILD_DIR:-${repo_root}/extras/test/build}"
test_binary="${MQTT_TEST_BINARY:-${test_build_dir}/supladevicetests}"
work_dir="${repo_root}/extras/docs/mqtt"
observed="${work_dir}/observed.json"
capture_tmp="$(mktemp "${work_dir}/.observed.XXXXXX.json")"
trap 'rm -f "${capture_tmp}"' EXIT

if [[ ! -x "${test_binary}" ]]; then
  cmake -S "${repo_root}/extras/test" -B "${test_build_dir}"
  CCACHE_DISABLE=1 cmake --build "${test_build_dir}" \
    --target supladevicetests -j2
fi

MQTT_DOC_CAPTURE="${capture_tmp}" \
  "${test_binary}" --gtest_filter='Mqtt*'
python3 -m json.tool "${capture_tmp}" >/dev/null
mv "${capture_tmp}" "${observed}"

python3 "${repo_root}/extras/tools/mqtt-docgen/render.py" \
  --observed "${observed}" \
  --metadata "${work_dir}/metadata.json" \
  --output "${repo_root}/docs/mqtt/topics.md" \
  --home-assistant-output \
    "${repo_root}/docs/mqtt/home-assistant-discovery.md"

python3 "${repo_root}/extras/tools/mqtt-docgen/render.py" \
  --observed "${observed}" \
  --metadata "${work_dir}/metadata.json" \
  --translations "${work_dir}/translations.pl.json" \
  --output "${repo_root}/docs/mqtt/topics.pl.md" \
  --home-assistant-output \
    "${repo_root}/docs/mqtt/home-assistant-discovery.pl.md"

python3 -m unittest "${repo_root}/extras/tools/mqtt-docgen/test_render.py"
