#!/usr/bin/env python3
"""Stage and verify S2 engine archives without installing or activating them."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path
from typing import Any, Mapping, Sequence


SETBFREE_SHA256 = "sha256:6ad090237b6fea0f777db13a52e044d91143df3496e5c7b1b4eccf479c4ba554"
SURGE_SHA256 = "sha256:dd431b75f5fa197c4bffa6ca27ca46970f0a94c834119bb1db7decdeec4c28db"


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return "sha256:" + value.hexdigest()


def safe_members(archive: tarfile.TarFile) -> Sequence[tarfile.TarInfo]:
    members = archive.getmembers()
    for member in members:
        if member.name.startswith("/") or ".." in Path(member.name).parts:
            raise ValueError(f"unsafe archive member: {member.name}")
    return members


def require_files(root: Path, paths: Sequence[str]) -> list[str]:
    missing = [path for path in paths if not (root / path).is_file()]
    if missing:
        raise ValueError(f"staged resource is missing {missing}")
    return list(paths)


def stage_setbfree(archive: Path, root: Path) -> Mapping[str, Any]:
    result = subprocess.run(
        ["dpkg-deb", "-x", str(archive), str(root)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise ValueError(f"setBfree extraction failed: {result.stderr}")
    files = require_files(root, (
        "usr/bin/setBfree",
        "usr/lib/lv2/b_synth/manifest.ttl",
        "usr/lib/lv2/b_synth/b_synth.ttl",
        "usr/share/setBfree/cfg/default.cfg",
        "usr/share/setBfree/pgm/default.pgm",
    ))
    return {
        "archive_sha256": digest(archive),
        "format": "deb-unpacked-temporary",
        "required_files": files,
        "state_assets": [
            "usr/share/setBfree/cfg/default.cfg",
            "usr/share/setBfree/pgm/default.pgm",
        ],
    }


def stage_surge(archive: Path, root: Path) -> Mapping[str, Any]:
    with tarfile.open(archive, "r:gz") as package:
        members = safe_members(package)
        package.extractall(root, members=members)
    files = require_files(root, (
        "Surge XT.lv2/manifest.ttl",
        "Surge XT.lv2/dsp.ttl",
        "Surge XT.lv2/libSurge XT.so",
        "surge-xt-cli",
    ))
    return {
        "archive_sha256": digest(archive),
        "format": "tar-gz-unpacked-temporary",
        "required_files": files,
        "state_assets": ["Surge XT.lv2/manifest.ttl"],
    }


def validate_fixture(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-engine-resource-stage/v1":
        raise ValueError("invalid S2 resource-stage schema")
    if document.get("activation") != "disabled":
        raise ValueError("resource evidence must remain activation-disabled")
    resources = document.get("resources")
    if not isinstance(resources, Mapping) or set(resources) != {"setbfree", "surge-xt"}:
        raise ValueError("both staged resources are required")
    if resources["setbfree"].get("archive_sha256") != SETBFREE_SHA256:
        raise ValueError("setBfree provenance hash changed")
    if resources["surge-xt"].get("archive_sha256") != SURGE_SHA256:
        raise ValueError("Surge XT provenance hash changed")
    runtime = document.get("runtime_checks")
    if not isinstance(runtime, Mapping) or runtime.get("status") != "blocked-by-safe-boundary":
        raise ValueError("runtime boundary is not recorded")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setbfree-package", type=Path)
    parser.add_argument("--surge-package", type=Path)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_fixture(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("S2 engine resource-stage fixture: PASS")
            return 0
        if arguments.setbfree_package is None or arguments.surge_package is None:
            parser.error("live staging requires both package paths")
        if digest(arguments.setbfree_package) != SETBFREE_SHA256:
            raise ValueError("setBfree archive hash does not match provenance")
        if digest(arguments.surge_package) != SURGE_SHA256:
            raise ValueError("Surge XT archive hash does not match provenance")
        with tempfile.TemporaryDirectory(prefix="music-rig-s2-resources-") as temporary:
            root = Path(temporary)
            setbfree = stage_setbfree(arguments.setbfree_package, root / "setbfree")
            surge = stage_surge(arguments.surge_package, root / "surge-xt")
            jack_host = shutil.which("jackd") or shutil.which("pw-jack")
            document = {
                "schema": "music-studies/s2-engine-resource-stage/v1",
                "activation": "disabled",
                "resources": {"setbfree": setbfree, "surge-xt": surge},
                "runtime_checks": {
                    "audio_cpu": "not-run",
                    "jack_host": jack_host,
                    "state_restore": "assets-staged-not-runtime-tested",
                    "status": "available" if jack_host else "blocked-by-safe-boundary",
                    "reason": None if jack_host else
                        "No isolated JACK host is available; PipeWire and protected Carla were not touched.",
                },
            }
        if arguments.output is not None:
            arguments.output.write_text(
                json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        print(json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
