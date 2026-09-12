#!/usr/bin/env python3
"""Run the real synthv1 prepared-process transaction on temporary JACK."""

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


def validate_fixture(document: dict) -> None:
    if document.get("schema") != "music-studies/s2-synthv1-process-runtime/v1":
        raise ValueError("invalid synthv1 process-runtime schema")
    if document.get("status") != "prepared-transaction-rollback-pass":
        raise ValueError("synthv1 process-runtime evidence is incomplete")
    if document.get("protected_graph_touched") is not False:
        raise ValueError("protected graph boundary was not preserved")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--runtime-test", type=Path)
    parser.add_argument("--synthv1", type=Path)
    parser.add_argument("--preset", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("synthv1 process-runtime fixture: PASS")
            return 0
        required = (
            arguments.jackd, arguments.jack_driver_dir, arguments.runtime_test,
            arguments.synthv1, arguments.preset,
        )
        if any(value is None for value in required):
            parser.error("live transaction requires all paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-synthv1-transaction-"):
            environment = os.environ.copy()
            environment["JACK_DRIVER_DIR"] = str(arguments.jack_driver_dir)
            environment["JACK_DEFAULT_SERVER"] = "music-rig-s2"
            jack = subprocess.Popen(
                [str(arguments.jackd), "-n", "music-rig-s2", "-d", "dummy",
                 "-r", "48000", "-p", "1024", "-C", "2", "-P", "2"],
                env=environment,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                time.sleep(1.0)
                result = subprocess.run(
                    [str(arguments.runtime_test), str(arguments.synthv1),
                     str(arguments.preset), "music-rig-s2"],
                    env=environment,
                    check=False,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                )
                if result.returncode != 0:
                    raise ValueError(f"runtime transaction failed: {result.stdout}")
            finally:
                jack.send_signal(signal.SIGTERM)
                try:
                    jack.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    jack.kill()
                    jack.wait()
        document = {
            "block_size": 1024,
            "preset": str(arguments.preset),
            "protected_graph_touched": False,
            "sample_rate": 48000,
            "schema": "music-studies/s2-synthv1-process-runtime/v1",
            "status": "prepared-transaction-rollback-pass",
            "synthv1": str(arguments.synthv1),
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
