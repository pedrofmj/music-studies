#!/usr/bin/env python3
"""Validate synthv1 direct ControlPort probe evidence."""

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
        if document.get("schema") != "music-studies/s2-synthv1-control-probe/v1":
            raise ValueError("invalid synthv1 probe schema")
        if document.get("control_count") != 145 or not document.get("finite_audio"):
            raise ValueError("synthv1 ControlPort/audio gate failed")
        if document.get("target_after") != 0.75:
            raise ValueError("synthv1 target response was not observed")
        if document.get("state_restore_status") == 0 or document.get("state_properties") != 0:
            raise ValueError("synthv1 state result was overstated")
        print("S2 synthv1 control probe fixture: PASS")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
