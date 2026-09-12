#!/usr/bin/env python3
"""Validate the candidate-only synthv1 controller configuration."""

from __future__ import annotations

import argparse
import configparser
import sys
from pathlib import Path


EXPECTED = {
    "Control_0_CC_16": "52,0",
    "Control_0_CC_17": "140,0",
    "Control_0_CC_18": "136,0",
    "Control_0_CC_19": "33,0",
    "Control_0_CC_71": "51,0",
    "Control_0_CC_72": "18,0",
    "Control_0_CC_73": "8,0",
    "Control_0_CC_74": "141,0",
    "Control_0_CC_75": "67,0",
    "Control_0_CC_76": "133,0",
    "Control_0_CC_77": "134,0",
    "Control_0_CC_79": "17,0",
    "Control_0_CC_80": "45,0",
    "Control_0_CC_81": "48,0",
    "Control_0_CC_82": "34,0",
    "Control_0_CC_83": "30,0",
    "Control_0_CC_85": "69,0",
    "Control_0_CC_93": "137,0",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    arguments = parser.parse_args()
    values = configparser.ConfigParser(interpolation=None)
    values.read(arguments.config, encoding="utf-8")
    try:
        if values.getboolean("Default", "ControlsEnabled") is not True:
            raise ValueError("synthv1 controls are not enabled")
        actual = dict(values.items("Controllers"))
        if actual != {key.lower(): value for key, value in EXPECTED.items()}:
            raise ValueError("candidate controller mapping differs from evidence")
    except (configparser.Error, KeyError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"synthv1 candidate controls: PASS ({len(EXPECTED)} mappings)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
