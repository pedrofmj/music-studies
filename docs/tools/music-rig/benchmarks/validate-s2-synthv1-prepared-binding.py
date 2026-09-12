#!/usr/bin/env python3
"""Validate the prepared-only synthv1 binding boundary."""

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
        if document.get("schema") != "music-studies/s2-synthv1-prepared-binding/v1":
            raise ValueError("invalid prepared-binding schema")
        if document.get("status") != "prepared-only" or document.get("activation") != "disabled":
            raise ValueError("synthv1 binding is not prepared-only")
        if document.get("live_platform_binding") is not False:
            raise ValueError("synthv1 binding entered a live platform")
        transaction = document.get("transaction", {})
        if transaction.get("commit") is not True or transaction.get("rollback") is not True:
            raise ValueError("prepared transaction evidence is incomplete")
        print("S2 synthv1 prepared-binding fixture: PASS")
        return 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
