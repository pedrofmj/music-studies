#!/usr/bin/env python3
"""Validate the isolated LV2 atom probe evidence."""

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
        if document.get("schema") != "music-studies/s2-lv2-atom-probe/v1":
            raise ValueError("invalid LV2 atom probe schema")
        if document.get("parameter_write_sent") != 1 or not document.get("finite_audio"):
            raise ValueError("LV2 atom write or finite-audio gate failed")
        if document.get("patch_response_events") != 0:
            raise ValueError("probe fixture overstated patch response evidence")
        if document.get("state_save_status") != 0 or document.get("state_restore_status") == 0:
            raise ValueError("state probe overstated restore success")
        if document.get("protected_graph_touched") is not False:
            raise ValueError("protected graph boundary was not preserved")
        print("S2 LV2 atom probe fixture: PASS")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
