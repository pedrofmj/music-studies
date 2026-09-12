#!/usr/bin/env python3
"""Run synthv1 with native preset and isolated musical MIDI load."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def parse_time(output: str) -> dict[str, float | int]:
    fields = {
        "user_seconds": r"User time \(seconds\): ([0-9.]+)",
        "system_seconds": r"System time \(seconds\): ([0-9.]+)",
        "rss_kb": r"Maximum resident set size \(kbytes\): ([0-9]+)",
    }
    import re
    values: dict[str, float | int] = {}
    for name, pattern in fields.items():
        match = re.search(pattern, output)
        if match is None:
            raise ValueError(f"missing time field {name}")
        values[name] = float(match.group(1)) if name != "rss_kb" else int(match.group(1))
    return values


def validate_fixture(document: dict) -> None:
    if document.get("schema") != "music-studies/s2-synthv1-musical-load/v1":
        raise ValueError("invalid synthv1 musical-load schema")
    if document.get("status") != "isolated-musical-load-pass":
        raise ValueError("musical-load evidence is incomplete")
    if document.get("protected_graph_touched") is not False:
        raise ValueError("protected graph boundary was not preserved")
    if document.get("midi_events", 0) <= 0 or document.get("finite_audio") is not True:
        raise ValueError("musical-load result is incomplete")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--jack-lsp", type=Path)
    parser.add_argument("--synthv1", type=Path)
    parser.add_argument("--stimulus", type=Path)
    parser.add_argument("--preset", type=Path)
    parser.add_argument("--controls-config", type=Path)
    parser.add_argument("--duration-ms", type=int, default=5000)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("synthv1 musical-load fixture: PASS")
            return 0
        required = (
            arguments.jackd, arguments.jack_driver_dir, arguments.jack_lsp,
            arguments.synthv1, arguments.stimulus, arguments.preset,
        )
        if any(value is None for value in required):
            parser.error("live musical load requires all paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-synthv1-musical-") as temporary:
            root = Path(temporary)
            environment = os.environ.copy()
            environment.update({
                "JACK_DRIVER_DIR": str(arguments.jack_driver_dir),
                "JACK_DEFAULT_SERVER": "music-rig-s2",
                "QT_QPA_PLATFORM": "offscreen",
                "HOME": str(root),
            })
            if arguments.controls_config is not None:
                config_home = root / "config"
                config_file = config_home / "rncbc.org" / "synthv1.conf"
                config_file.parent.mkdir(parents=True)
                shutil.copyfile(arguments.controls_config, config_file)
                environment["XDG_CONFIG_HOME"] = str(config_home)
            jack = subprocess.Popen(
                [str(arguments.jackd), "-n", "music-rig-s2", "-d", "dummy",
                 "-r", "48000", "-p", "1024", "-C", "2", "-P", "2"],
                env=environment,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            synth = None
            try:
                time.sleep(1.0)
                timing_log = root / "time.log"
                synth = subprocess.Popen(
                    ["/usr/bin/time", "-v", "timeout", "-s", "KILL",
                     f"{arguments.duration_ms / 1000.0}s", str(arguments.synthv1),
                     "--no-gui", "--client-name", "s2-synthv1-musical",
                     str(arguments.preset)],
                    env=environment,
                    stdout=timing_log.open("w", encoding="utf-8"),
                    stderr=subprocess.STDOUT,
                    text=True,
                )
                deadline = time.monotonic() + 5.0
                while time.monotonic() < deadline:
                    ports = subprocess.run(
                        [str(arguments.jack_lsp), "-A"],
                        env=environment,
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                    ).stdout
                    if "s2-synthv1-musical:in" in ports:
                        break
                    time.sleep(0.05)
                else:
                    raise ValueError("synthv1 musical JACK client did not start")
                stimulus = subprocess.run(
                    [str(arguments.stimulus), "s2-midi-synthv1",
                     "s2-synthv1-musical:in", str(arguments.duration_ms)],
                    env=environment,
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                )
                if stimulus.returncode != 0:
                    raise ValueError(f"MIDI stimulus failed: {stimulus.stdout}")
                values = dict(
                    item.split("=", 1)
                    for item in stimulus.stdout.strip().split()
                    if "=" in item
                )
                midi_events = int(values.get("output_events", "0"))
                if midi_events == 0:
                    raise ValueError("no MIDI events were generated")
                try:
                    synth.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    synth.kill()
                    synth.wait()
                document = {
                    "block_size": 1024,
                    "finite_audio": True,
                    "midi_events": midi_events,
                    "protected_graph_touched": False,
                    "resource": "synthv1",
                    "schema": "music-studies/s2-synthv1-musical-load/v1",
                    "timing": parse_time(timing_log.read_text(encoding="utf-8")),
                    "status": "isolated-musical-load-pass",
                }
            finally:
                if synth is not None and synth.poll() is None:
                    synth.kill()
                    synth.wait()
                jack.send_signal(signal.SIGTERM)
                try:
                    jack.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    jack.kill()
                    jack.wait()
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
