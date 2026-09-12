#!/usr/bin/env python3
"""Guarded candidate-only activation boundary using temporary JACK only."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def validate_fixture(document: dict) -> None:
    if document.get("schema") != "music-studies/s2-candidate-boundary/v1":
        raise ValueError("invalid candidate-boundary schema")
    if document.get("status") != "candidate-only-boundary-pass":
        raise ValueError("candidate boundary did not pass")
    if document.get("physical_endpoints") or document.get("pipewire_touched"):
        raise ValueError("candidate boundary crossed a protected endpoint")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile")
    parser.add_argument("--binding")
    parser.add_argument("--acknowledge-candidate-only", action="store_true")
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--jackd", type=Path)
    parser.add_argument("--jack-driver-dir", type=Path)
    parser.add_argument("--jack-lsp", type=Path)
    parser.add_argument("--synthv1", type=Path)
    parser.add_argument("--stimulus", type=Path)
    parser.add_argument("--preset", type=Path)
    parser.add_argument("--controls-config", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("S2 candidate-boundary fixture: PASS")
            return 0
        if not arguments.acknowledge_candidate_only:
            raise ValueError("candidate-only acknowledgement is required")
        if arguments.profile != "synth-programmer-synthv1" or arguments.binding != "synthv1-candidate":
            raise ValueError("only the synthv1 candidate binding may use this boundary")
        required = (
            arguments.runner, arguments.jackd, arguments.jack_driver_dir,
            arguments.jack_lsp, arguments.synthv1, arguments.stimulus,
            arguments.preset, arguments.controls_config,
        )
        if any(value is None for value in required):
            parser.error("live candidate boundary requires all runner paths")
        with tempfile.TemporaryDirectory(prefix="music-rig-s2-boundary-") as temporary:
            output = Path(temporary) / "musical-load.json"
            result = subprocess.run(
                [sys.executable, str(arguments.runner),
                 "--jackd", str(arguments.jackd),
                 "--jack-driver-dir", str(arguments.jack_driver_dir),
                 "--jack-lsp", str(arguments.jack_lsp),
                 "--synthv1", str(arguments.synthv1),
                  "--stimulus", str(arguments.stimulus),
                  "--preset", str(arguments.preset),
                  "--controls-config", str(arguments.controls_config),
                  "--output", str(output)],
                check=False, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            if result.returncode != 0:
                raise ValueError(result.stdout)
            musical = json.loads(output.read_text(encoding="utf-8"))
        document = {
            "binding": arguments.binding,
            "physical_endpoints": False,
            "pipewire_touched": False,
            "profile": arguments.profile,
            "schema": "music-studies/s2-candidate-boundary/v1",
            "status": "candidate-only-boundary-pass",
            "musical_load": musical,
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
