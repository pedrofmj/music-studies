#!/usr/bin/env python3
"""Validate complete, non-activating S2 engine-control contracts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping


ROOT = Path(__file__).resolve().parents[4]
RIG_ROOT = ROOT / "src" / "performance-rigs" / "pedro-performance-rig"


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def profile_targets(profile: str) -> set[str]:
    document = load_json(RIG_ROOT / "device-profiles" / "arturia-main" / f"{profile}.json")
    return {mapping["target"] for mapping in document["mappings"]}


def validate(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-engine-control-contract/v1":
        raise ValueError("invalid S2 engine-control contract schema")
    if document.get("status") != "prepared-contract-only" or document.get("activation_eligible"):
        raise ValueError("S2 engine contract must remain non-activating")
    candidates = document.get("candidates")
    if not isinstance(candidates, Mapping) or set(candidates) != {"setbfree", "surge-xt"}:
        raise ValueError("both candidate engine contracts are required")
    expected = {
        "setbfree": profile_targets("tonewheel-organ"),
        "surge-xt": profile_targets("synth-programmer"),
    }
    for engine, targets in expected.items():
        candidate = candidates[engine]
        if not isinstance(candidate, Mapping):
            raise ValueError(f"{engine} contract is not an object")
        if engine == "surge-xt" and candidate.get("activation_status") != "unavailable-host-binding":
            raise ValueError("Surge XT host binding must remain unavailable")
        contract_targets = candidate.get("targets")
        if not isinstance(contract_targets, Mapping) or set(contract_targets) != targets:
            raise ValueError(f"{engine} contract does not cover every profile target")
        for target, binding in contract_targets.items():
            if not isinstance(binding, Mapping):
                raise ValueError(f"{engine}/{target} binding is not an object")
            status = binding.get("status")
            if status == "unavailable":
                if "backend_control" in binding or "backend_parameter" in binding:
                    raise ValueError(f"unavailable target has a backend binding: {target}")
            elif status not in {
                "verified-control-surface",
                "metadata-only",
                "metadata-only-routing-open",
            }:
                raise ValueError(f"unknown binding status for {target}: {status}")
            elif not ("backend_control" in binding or "backend_parameter" in binding):
                raise ValueError(f"verified target has no backend binding: {target}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    arguments = parser.parse_args()
    try:
        validate(load_json(arguments.fixture))
        print("S2 engine-control contract: PASS")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
