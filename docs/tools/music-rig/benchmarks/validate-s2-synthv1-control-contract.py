#!/usr/bin/env python3
"""Validate the synthv1 semantic-control fallback contract."""

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
        if document.get("schema") != "music-studies/s2-synth-control-contract/v1":
            raise ValueError("invalid synthv1 contract schema")
        if document.get("candidate") != "synthv1" or document.get("status") != "prepared-contract-only":
            raise ValueError("synthv1 contract must remain prepared-only")
        if document.get("activation_status") != "prepared-only-native-preset" or document.get("live_activation") is not False:
            raise ValueError("synthv1 activation boundary is invalid")
        state_resource = document.get("state_resource")
        if not isinstance(state_resource, dict) or state_resource.get("strategy") != "native-synthv1-preset-file" or not state_resource.get("accepted_for_prepared_state"):
            raise ValueError("synthv1 native preset state strategy is missing")
        targets = document.get("targets", {})
        if len(targets) != 18:
            raise ValueError("synthv1 contract must cover all 18 synth targets")
        for target, binding in targets.items():
            if binding.get("status") == "unavailable":
                if "backend_parameter" in binding:
                    raise ValueError(f"unavailable target has a backend binding: {target}")
            elif binding.get("status") != "verified-metadata" or "backend_parameter" not in binding:
                raise ValueError(f"invalid synthv1 binding: {target}")
        print("S2 synthv1 control contract: PASS")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
