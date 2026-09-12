#!/usr/bin/env python3
"""Run synthv1 with a native preset in temporary JACK and verify ownership."""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import tempfile
import time
from pathlib import Path


def validate_fixture(document: dict) -> None:
    if document.get("schema") != "music-studies/s2-synthv1-preset-process/v1":
        raise ValueError("invalid synthv1 process schema")
    if document.get("protected_graph_touched") is not False:
        raise ValueError("protected graph boundary was not preserved")
    if document.get("status") != "preset-loaded-process-owned-cleaned":
        raise ValueError("preset process evidence is incomplete")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--jack-lsp", type=Path)
    parser.add_argument("--synthv1", type=Path)
    parser.add_argument("--preset", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("synthv1 preset-process fixture: PASS")
            return 0
        required = (
            arguments.jackd,
            arguments.jack_driver_dir,
            arguments.jack_lsp,
            arguments.synthv1,
            arguments.preset,
        )
        if any(value is None for value in required):
            parser.error("live process validation requires all paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-synthv1-process-") as temporary:
            root = Path(temporary)
            log_path = root / "synthv1.log"
            environment = os.environ.copy()
            environment.update({
                "JACK_DRIVER_DIR": str(arguments.jack_driver_dir),
                "JACK_DEFAULT_SERVER": "music-rig-s2",
                "QT_QPA_PLATFORM": "offscreen",
                "HOME": str(root),
            })
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
                synth = subprocess.Popen(
                    [str(arguments.synthv1), "--no-gui", "--client-name",
                     "s2-synthv1-preset", str(arguments.preset)],
                    env=environment,
                    stdout=log_path.open("w", encoding="utf-8"),
                    stderr=subprocess.STDOUT,
                )
                deadline = time.monotonic() + 5.0
                ports = ""
                while time.monotonic() < deadline:
                    probe = subprocess.run(
                        [str(arguments.jack_lsp), "-A"],
                        env=environment,
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                    )
                    ports = probe.stdout
                    if "s2-synthv1-preset" in ports:
                        break
                    time.sleep(0.05)
                else:
                    raise ValueError(f"synthv1 JACK client did not appear: {ports}")
                time.sleep(2.0)
                if synth.poll() is not None:
                    raise ValueError(f"synthv1 exited early: {synth.returncode}")
                log = log_path.read_text(encoding="utf-8")
                if "error" in log.lower() and "realtime" not in log.lower():
                    raise ValueError(f"synthv1 reported an error: {log}")
                document = {
                    "preset": str(arguments.preset),
                    "protected_graph_touched": False,
                    "schema": "music-studies/s2-synthv1-preset-process/v1",
                    "status": "preset-loaded-process-owned-cleaned",
                    "temporary_jack": {"sample_rate": 48000, "block_size": 1024},
                }
            finally:
                if synth is not None and synth.poll() is None:
                    synth.send_signal(signal.SIGTERM)
                    try:
                        synth.wait(timeout=3.0)
                    except subprocess.TimeoutExpired:
                        synth.kill()
                        synth.wait()
                if jack.poll() is None:
                    jack.send_signal(signal.SIGTERM)
                    try:
                        jack.wait(timeout=3.0)
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
        print(f"FAIL: {error}", file=__import__("sys").stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
