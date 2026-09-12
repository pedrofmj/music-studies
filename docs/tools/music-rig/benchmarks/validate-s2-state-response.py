#!/usr/bin/env python3
"""Validate S2 isolated state/control-response evidence."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Mapping


def validate(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-state-response/v1":
        raise ValueError("invalid S2 state-response schema")
    if document.get("protected_graph_touched") is not False:
        raise ValueError("protected graph boundary was not preserved")
    candidates = document.get("candidates")
    if not isinstance(candidates, Mapping) or set(candidates) != {"setbfree", "surge-xt"}:
        raise ValueError("both candidate results are required")
    for candidate in candidates.values():
        if candidate.get("status") != "plugin-alive-under-isolated-MIDI":
            raise ValueError("candidate did not survive isolated MIDI response test")
        if "not-save-tested" not in candidate.get("state_restore", ""):
            raise ValueError("state restore was overstated")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    arguments = parser.parse_args()
    try:
        validate(json.loads(arguments.fixture.read_text(encoding="utf-8")))
        print("S2 state-response fixture: PASS")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
