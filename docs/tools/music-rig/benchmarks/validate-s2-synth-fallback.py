#!/usr/bin/env python3
"""Validate the isolated synthv1 fallback evidence."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Mapping


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    arguments = parser.parse_args()
    try:
        document: Mapping[str, object] = json.loads(
            arguments.fixture.read_text(encoding="utf-8")
        )
        surface = document["control_surface"]
        if document.get("schema") != "music-studies/s2-synth-fallback/v1":
            raise ValueError("invalid synthv1 fallback schema")
        if document.get("activation") != "disabled" or document.get("candidate") != "synthv1":
            raise ValueError("synthv1 fallback must remain activation-disabled")
        if not isinstance(surface, Mapping) or surface.get("parameter_inputs") != 145:
            raise ValueError("synthv1 control-port inventory changed")
        if not surface.get("ordinary_lv2_control_ports") or not document.get("state_interface"):
            raise ValueError("synthv1 host-control/state contract is incomplete")
        print("S2 synthv1 fallback fixture: PASS")
        return 0
    except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
