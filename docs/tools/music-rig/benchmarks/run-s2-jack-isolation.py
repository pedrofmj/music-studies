#!/usr/bin/env python3
"""Run staged S2 LV2 candidates in a temporary JACK dummy server."""

from __future__ import annotations

import argparse
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Mapping


TIME_FIELDS = {
    "user": r"User time \(seconds\): ([0-9.]+)",
    "system": r"System time \(seconds\): ([0-9.]+)",
    "rss_kb": r"Maximum resident set size \(kbytes\): ([0-9]+)",
}


def parse_time(output: str) -> Mapping[str, Any]:
    values: dict[str, Any] = {}
    for name, pattern in TIME_FIELDS.items():
        match = re.search(pattern, output)
        if match is None:
            raise ValueError(f"missing /usr/bin/time field: {name}")
        values[name] = float(match.group(1)) if name != "rss_kb" else int(match.group(1))
    return values


def run_plugin(
    jack_env: Mapping[str, str],
    jalv: Path,
    plugin_name: str,
    plugin_uri: str,
    lv2_path: Path,
    library_path: Path,
    duration: int,
) -> Mapping[str, Any]:
    environment = dict(jack_env)
    environment["LV2_PATH"] = str(lv2_path)
    environment["LD_LIBRARY_PATH"] = str(library_path)
    result = subprocess.run(
        [
            "/usr/bin/time",
            "-v",
            "timeout",
            "-s",
            "INT",
            f"{duration}s",
            str(jalv),
            "-i",
            "-n",
            f"s2-{plugin_name}",
            plugin_uri,
        ],
        env=environment,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 124:
        raise ValueError(
            f"{plugin_name} did not complete the bounded isolation run: "
            f"status={result.returncode}\n{result.stdout}"
        )
    for marker in ("Plugin:", "JACK Name:", "Block length: 1024 frames"):
        if marker not in result.stdout:
            raise ValueError(f"{plugin_name} isolation output omitted {marker}")
    for forbidden in ("failed to instantiate", "LV2 plugin not found", "lilv_world_load_bundle"):
        if forbidden in result.stdout:
            raise ValueError(f"{plugin_name} isolation reported {forbidden}")
    timing = parse_time(result.stdout)
    return {
        "audio": {"sample_rate": 48000, "block_size": 1024},
        "duration_seconds": duration,
        "plugin_uri": plugin_uri,
        "resource": plugin_name,
        "timing": timing,
        "status": "loaded-and-processed-idle-audio",
    }


def validate_fixture(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-jack-isolation/v1":
        raise ValueError("invalid S2 JACK-isolation schema")
    if document.get("server", {}).get("backend") != "dummy":
        raise ValueError("S2 isolation must use the JACK dummy backend")
    results = document.get("results")
    if not isinstance(results, Mapping) or set(results) != {"setbfree", "surge-xt"}:
        raise ValueError("both S2 candidates are required")
    for result in results.values():
        if result.get("status") != "loaded-and-processed-idle-audio":
            raise ValueError("candidate did not pass isolated load status")
        if result.get("audio") != {"sample_rate": 48000, "block_size": 1024}:
            raise ValueError("protected audio workload changed")
    if document.get("state_restore") != "not-runtime-tested":
        raise ValueError("state restore must remain explicitly open")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--jack-lsp", type=Path)
    parser.add_argument("--jalv", type=Path)
    parser.add_argument("--setbfree-lv2", type=Path)
    parser.add_argument("--surge-lv2", type=Path)
    parser.add_argument("--suil-library-dir", type=Path)
    parser.add_argument("--duration", type=int, default=5)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("S2 JACK-isolation fixture: PASS")
            return 0
        required = (
            arguments.jackd,
            arguments.jack_driver_dir,
            arguments.jack_lsp,
            arguments.jalv,
            arguments.setbfree_lv2,
            arguments.surge_lv2,
            arguments.suil_library_dir,
        )
        if any(value is None for value in required):
            parser.error("live isolation requires all host and bundle paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-s2-jack-") as temporary:
            log = Path(temporary) / "jackd.log"
            environment = os.environ.copy()
            environment["JACK_DRIVER_DIR"] = str(arguments.jack_driver_dir)
            server = subprocess.Popen(
                [
                    str(arguments.jackd),
                    "-n", "music-rig-s2",
                    "-d", "dummy",
                    "-r", "48000",
                    "-p", "1024",
                    "-C", "2",
                    "-P", "2",
                ],
                env=environment,
                stdout=log.open("w", encoding="utf-8"),
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                jack_environment = dict(environment)
                jack_environment["JACK_DEFAULT_SERVER"] = "music-rig-s2"
                deadline = time.monotonic() + 5.0
                while time.monotonic() < deadline:
                    probe = subprocess.run(
                        [str(arguments.jack_lsp), "-A"],
                        env=jack_environment,
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                    )
                    if probe.returncode == 0 and "system:playback_1" in probe.stdout:
                        break
                    time.sleep(0.05)
                else:
                    raise ValueError(f"temporary JACK server did not start: {log.read_text()}")
                surge_lv2_root = Path(temporary) / "surge-lv2"
                surge_lv2_root.mkdir()
                (surge_lv2_root / "Surge XT.lv2").symlink_to(
                    arguments.surge_lv2 / "Surge XT.lv2",
                    target_is_directory=True,
                )
                results = {
                    "setbfree": run_plugin(
                        jack_environment,
                        arguments.jalv,
                        "setbfree",
                        "http://gareus.org/oss/lv2/b_synth",
                        arguments.setbfree_lv2,
                        arguments.suil_library_dir,
                        arguments.duration,
                    ),
                    "surge-xt": run_plugin(
                        jack_environment,
                        arguments.jalv,
                        "surge-xt",
                        "https://surge-synthesizer.github.io/lv2/surge-xt",
                        surge_lv2_root,
                        arguments.suil_library_dir,
                        arguments.duration,
                    ),
                }
            finally:
                server.send_signal(signal.SIGTERM)
                try:
                    server.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
        document = {
            "schema": "music-studies/s2-jack-isolation/v1",
            "server": {"backend": "dummy", "sample_rate": 48000, "block_size": 1024},
            "results": results,
            "state_restore": "not-runtime-tested",
            "protected_graph_touched": False,
        }
        if arguments.output is not None:
            arguments.output.write_text(
                json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        print(json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
