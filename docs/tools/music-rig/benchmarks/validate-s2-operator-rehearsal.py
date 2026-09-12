#!/usr/bin/env python3
"""Validate the offline S2 operator rehearsal evidence."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    arguments = parser.parse_args()
    try:
        document = json.loads(arguments.fixture.read_text(encoding="utf-8"))
        if document.get("schema") != "music-studies/s2-operator-rehearsal/v1":
            raise ValueError("invalid operator rehearsal schema")
        if document.get("status") != "offline-rehearsal-pass":
            raise ValueError("offline rehearsal did not pass")
        if document.get("live_activation") is not False:
            raise ValueError("offline rehearsal claims live activation")
        safeguards = document.get("safeguards", {})
        if any(safeguards.get(key) is not False for key in (
            "hardware_touched", "services_changed", "pipewire_graph_changed",
            "protected_artifacts_changed"
        )):
            raise ValueError("offline rehearsal safety boundary failed")
        expected_steps = {
            "compile-organ",
            "compile-synth",
            "candidate-binding",
            "runtime-dry-run",
            "commit-switchback",
            "preset-stage-process",
        }
        if set(document.get("steps", [])) != expected_steps:
            raise ValueError("offline rehearsal step coverage is incomplete")
        print("S2 offline operator rehearsal: PASS")
        return 0
    except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
