#!/usr/bin/env python3
"""Stage and validate a synthv1 native preset as a temporary state resource."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Mapping
from xml.etree import ElementTree


ARCHIVE_SHA256 = "sha256:16391911dbfabbcfde9d4fe1fad0987b4710ede27ffc5f71d3c9e0b05fa5803e"
PRESET_NAME = "50_SynthRemember.synthv1"
REQUIRED_PARAMS = {
    "DCO1_BALANCE",
    "DCO2_BALANCE",
    "DCF1_CUTOFF",
    "DCF1_RESO",
    "DCA1_ATTACK",
    "DCA1_RELEASE",
    "LFO1_BALANCE",
    "LFO1_RATE",
    "OUT1_VOLUME",
}


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return "sha256:" + value.hexdigest()


def validate_fixture(document: Mapping[str, object]) -> None:
    if document.get("schema") != "music-studies/s2-synthv1-preset-stage/v1":
        raise ValueError("invalid synthv1 preset-stage schema")
    if document.get("activation") != "disabled" or document.get("parameter_count") != 145:
        raise ValueError("synthv1 preset staging evidence is incomplete")
    if document.get("archive_sha256") != ARCHIVE_SHA256:
        raise ValueError("synthv1 preset archive provenance changed")
    if document.get("prepared_state_acceptance") != "accepted-native-preset":
        raise ValueError("native preset was not accepted for prepared state")
    if not REQUIRED_PARAMS <= set(document.get("required_parameters", [])):
        raise ValueError("synthv1 preset omits a candidate control")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("synthv1 preset-stage fixture: PASS")
            return 0
        if arguments.archive is None:
            parser.error("live staging requires --archive")
        if digest(arguments.archive) != ARCHIVE_SHA256:
            raise ValueError("synthv1 preset archive hash does not match provenance")
        with zipfile.ZipFile(arguments.archive) as archive:
            member = archive.getinfo(PRESET_NAME)
            if member.filename != PRESET_NAME:
                raise ValueError("unexpected synthv1 preset member")
            with tempfile.TemporaryDirectory(prefix="music-rig-synthv1-state-") as root:
                preset = Path(root) / PRESET_NAME
                preset.write_bytes(archive.read(member))
                document = ElementTree.parse(preset).getroot()
                parameters = {
                    item.attrib["name"]
                    for item in document.findall("./params/param")
                }
                result = {
                    "activation": "disabled",
                    "archive_sha256": digest(arguments.archive),
                    "parameter_count": len(parameters),
                    "preset_sha256": digest(preset),
                    "required_parameters": sorted(REQUIRED_PARAMS),
                    "state_strategy": "native-synthv1-preset-file",
                    "prepared_state_acceptance": "accepted-native-preset",
                    "status": "temporary-stage-validated",
                    "schema": "music-studies/s2-synthv1-preset-stage/v1",
                }
                if not REQUIRED_PARAMS <= parameters:
                    raise ValueError("synthv1 preset omits a candidate control")
        if arguments.output is not None:
            arguments.output.write_text(
                json.dumps(result, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        print(json.dumps(result, ensure_ascii=True, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, ElementTree.ParseError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
