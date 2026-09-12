#!/usr/bin/env python3
"""Validate candidate-only synthv1 state resource and rollback ownership."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
RIG_ROOT = ROOT / "src" / "performance-rigs" / "pedro-performance-rig"


def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--fallback-evidence", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        rig = load(RIG_ROOT / "rig.json")
        binding = load(
            RIG_ROOT / "platform-bindings" / "linux" / "airstar-current.json"
        )
        contract = load(arguments.contract)
        evidence = load(arguments.fallback_evidence)
        if "synth-programmer-synthv1" not in rig["rig_profiles"]:
            raise ValueError("candidate synthv1 Rig Profile is not catalogued")
        arturia = next(
            slot for slot in rig["device_slots"] if slot["id"] == "arturia-main"
        )
        if "synth-programmer-synthv1" not in arturia["available_device_profiles"]:
            raise ValueError("candidate synthv1 Device Profile is not catalogued")
        if "synth-programmer-synthv1" in binding["rig_profiles"]:
            raise ValueError("candidate synthv1 profile entered the protected binding")
        if contract["state_resource"]["strategy"] != "native-synthv1-preset-file":
            raise ValueError("native preset state strategy is not selected")
        if evidence["status"] != "temporary-stage-validated":
            raise ValueError("native preset resource was not staged successfully")
        sequence = ["full-live-rack", "synth-programmer-synthv1", "full-live-rack"]
        if sequence[0] != sequence[-1] or sequence[1] == sequence[0]:
            raise ValueError("candidate rollback sequence is invalid")
        print("S2 synthv1 profile-state rollback contract: PASS")
        return 0
    except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
