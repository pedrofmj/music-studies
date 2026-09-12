#!/usr/bin/env python3
"""Stimulate staged S2 LV2 candidates through an isolated JACK MIDI client."""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Mapping


def run_candidate(
    arguments: argparse.Namespace,
    server_environment: Mapping[str, str],
    plugin_name: str,
    plugin_uri: str,
    lv2_path: Path,
    target: str,
    source: str | None,
) -> Mapping[str, Any]:
    environment = dict(server_environment)
    environment["LV2_PATH"] = str(lv2_path)
    environment["LD_LIBRARY_PATH"] = str(arguments.suil_library_dir)
    log_path = Path(arguments.output.parent if arguments.output else ".") / (
        f".s2-{plugin_name}-midi.log"
    )
    with log_path.open("w", encoding="utf-8") as log:
        plugin = subprocess.Popen(
            [str(arguments.jalv), "-i", "-n", f"s2-{plugin_name}", plugin_uri],
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            time.sleep(2.0)
            command = [
                str(arguments.stimulus),
                f"s2-midi-{plugin_name}",
                target,
                str(arguments.duration_ms),
            ]
            if source is not None:
                command.append(source)
            stimulus = subprocess.run(
                command,
                env=server_environment,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            if stimulus.returncode != 0:
                ports = subprocess.run(
                    [str(arguments.jack_lsp), "-A"],
                    env=server_environment,
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                )
                raise ValueError(
                    f"{plugin_name} MIDI stimulus failed: {stimulus.stdout}\n"
                    f"ports:\n{ports.stdout}\nlog:\n{log_path.read_text(encoding='utf-8')}"
                )
            values = dict(
                item.split("=", 1)
                for item in stimulus.stdout.strip().split()
                if "=" in item
            )
            output_events = int(values.get("output_events", "0"))
            input_events = int(values.get("input_events", "0"))
            if output_events == 0:
                raise ValueError(f"{plugin_name} received no generated MIDI events")
            if plugin.poll() is not None:
                raise ValueError(f"{plugin_name} exited during MIDI stimulus")
            return {
                "duration_ms": arguments.duration_ms,
                "input_feedback_events": input_events,
                "output_midi_events": output_events,
                "plugin_uri": plugin_uri,
                "status": "midi-stimulus-delivered-plugin-alive",
            }
        finally:
            plugin.send_signal(signal.SIGTERM)
            try:
                plugin.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                plugin.kill()
                plugin.wait()
            log_path.unlink(missing_ok=True)


def validate_fixture(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-jack-midi/v1":
        raise ValueError("invalid S2 JACK MIDI schema")
    if document.get("protected_graph_touched") is not False:
        raise ValueError("protected graph was not proven untouched")
    for result in document.get("results", {}).values():
        if result.get("status") != "midi-stimulus-delivered-plugin-alive":
            raise ValueError("MIDI candidate result is not healthy")
        if result.get("output_midi_events", 0) <= 0:
            raise ValueError("MIDI stimulus was empty")
    if document.get("state_restore") != "not-runtime-tested":
        raise ValueError("state restore must remain explicitly open")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--jack-lsp", type=Path)
    parser.add_argument("--jalv", type=Path)
    parser.add_argument("--stimulus", type=Path)
    parser.add_argument("--setbfree-lv2", type=Path)
    parser.add_argument("--surge-lv2", type=Path)
    parser.add_argument("--suil-library-dir", type=Path)
    parser.add_argument("--duration-ms", type=int, default=5000)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("S2 JACK MIDI fixture: PASS")
            return 0
        required = (
            arguments.jackd, arguments.jack_driver_dir, arguments.jack_lsp,
            arguments.jalv, arguments.stimulus, arguments.setbfree_lv2,
            arguments.surge_lv2, arguments.suil_library_dir,
        )
        if any(value is None for value in required):
            parser.error("live MIDI validation requires all host and bundle paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-s2-midi-") as temporary:
            root = Path(temporary)
            log = root / "jackd.log"
            environment = os.environ.copy()
            environment["JACK_DRIVER_DIR"] = str(arguments.jack_driver_dir)
            server = subprocess.Popen(
                [str(arguments.jackd), "-n", "music-rig-s2", "-d", "dummy",
                 "-r", "48000", "-p", "1024", "-C", "2", "-P", "2"],
                env=environment,
                stdout=log.open("w", encoding="utf-8"),
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                server_environment = dict(environment)
                server_environment["JACK_DEFAULT_SERVER"] = "music-rig-s2"
                deadline = time.monotonic() + 5.0
                while time.monotonic() < deadline:
                    probe = subprocess.run(
                        [str(arguments.jack_lsp), "-A"],
                        env=server_environment,
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
                surge_root = root / "surge-lv2"
                surge_root.mkdir()
                (surge_root / "Surge XT.lv2").symlink_to(
                    arguments.surge_lv2 / "Surge XT.lv2", target_is_directory=True
                )
                results = {
                    "setbfree": run_candidate(
                        arguments, server_environment, "setbfree",
                        "http://gareus.org/oss/lv2/b_synth",
                        arguments.setbfree_lv2, "s2-setbfree:control",
                        "s2-setbfree:notify",
                    ),
                }
                time.sleep(0.5)
                results["surge-xt"] = run_candidate(
                        arguments, server_environment, "surge-xt",
                        "https://surge-synthesizer.github.io/lv2/surge-xt",
                        surge_root, "s2-surge-xt:in", None,
                )
            finally:
                server.send_signal(signal.SIGTERM)
                try:
                    server.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
        document = {
            "schema": "music-studies/s2-jack-midi/v1",
            "protected_graph_touched": False,
            "results": results,
            "state_restore": "not-runtime-tested",
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
