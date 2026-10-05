#!/usr/bin/env python3
# SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the SERVER bootstrap hardware fixture on both Arduino ESP targets."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

TARGETS = {
    "esp8266": "esp8266:esp8266:d1_mini",
    "esp32": "esp32:esp32:esp32wrover:PartitionScheme=huge_app",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", action="append", choices=TARGETS)
    parser.add_argument("--arduino-cli", default="arduino-cli")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--sketch", choices=("ServerBootstrap", "SupLanD2D"),
                        default="ServerBootstrap")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    repo = Path(__file__).resolve().parents[3]
    root = Path(tempfile.mkdtemp(prefix="suplan-arduino-", dir="/tmp/codex"))
    (root / "tmp").mkdir()
    sketch = root / args.sketch
    shutil.copytree(Path(__file__).parent / args.sketch, sketch)
    if args.sketch == "SupLanD2D":
        shutil.copyfile(repo / "extras/examples/suplan_poc1_common/" /
                        "suplan_poc1_credentials.h",
                        sketch / "suplan_poc1_credentials.h")
    config = root / "cli.yaml"
    config.write_text("build_cache:\n  path: " + str(root / "cache") +
                      "\ndirectories:\n  downloads: " +
                      str(root / "downloads") + "\n", encoding="utf-8")
    env = dict(os.environ, TMPDIR=str(root / "tmp"))
    results = {}
    print("Artifacts: " + str(root), flush=True)
    version = subprocess.check_output([args.arduino_cli, "version"],
                                      text=True, env=env).strip()
    for platform in args.platform or list(TARGETS):
        command = [args.arduino_cli, "--config-file", str(config), "compile",
                   "--fqbn", TARGETS[platform], "--jobs", str(args.jobs),
                   "--build-path", str(root / ("build-" + platform)),
                   "--output-dir", str(root / ("out-" + platform)),
                   "--library", str(repo), str(sketch)]
        log = root / (platform + ".log")
        with log.open("w") as output:
            completed = subprocess.run(command, cwd=repo, env=env,
                                       stdout=output, stderr=subprocess.STDOUT)
        results[platform] = {"fqbn": TARGETS[platform],
                             "returncode": completed.returncode,
                             "command": command, "log": str(log)}
        print(platform + ": " + ("PASS" if completed.returncode == 0 else
                                  "FAIL") + " (" + str(log) + ")", flush=True)
    (root / "results.json").write_text(
        json.dumps({"cli": version, "results": results}, indent=2) + "\n",
        encoding="utf-8")
    return int(any(result["returncode"] for result in results.values()))


if __name__ == "__main__":
    raise SystemExit(main())
