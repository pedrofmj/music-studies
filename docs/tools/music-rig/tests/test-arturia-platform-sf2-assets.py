#!/usr/bin/env python3
"""Check Android patch overrides preserve the shared Arturia routing contract."""

from __future__ import annotations

import json
from pathlib import Path
import sys


SCRIPT_PATH = Path(__file__).resolve()
TOOLS_ROOT = SCRIPT_PATH.parents[1]
BENCHMARKS = TOOLS_ROOT / "benchmarks"
sys.path.insert(0, str(BENCHMARKS))
sys.path.insert(0, str(TOOLS_ROOT / "packaging"))

from arturia_shared_sf2_catalog import build_profiles  # noqa: E402
from package_arturia_sf2_pack import PLATFORM_OVERRIDE_SCHEMA, normalize_catalog_path  # noqa: E402


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    catalog = load_json(BENCHMARKS / "arturia-genre-patches.json")
    routing = load_json(BENCHMARKS / "arturia-shared-sf2-routing.json")
    overrides = load_json(BENCHMARKS / "arturia-sf2-assets.android.json")
    patch_ids, _paths, _profiles = build_profiles(catalog, routing)

    if overrides.get("schema") != PLATFORM_OVERRIDE_SCHEMA or \
            overrides.get("platform") != "android" or \
            overrides.get("source_catalog") != "arturia-genre-patches.json":
        raise ValueError("Android platform asset override contract differs")
    if set(catalog["genres"]) != set(routing["active_layers"]):
        raise ValueError("base catalog and shared routing profiles differ")

    patches = overrides.get("patches")
    if not isinstance(patches, dict) or not patches:
        raise ValueError("Android platform override list is empty")
    unknown = set(patches) - set(patch_ids)
    if unknown:
        raise ValueError("Android overrides reference inactive patches: " +
                         ", ".join(sorted(unknown)))

    logical_paths = {}
    for patch_id, entry in patches.items():
        if not isinstance(entry, dict):
            raise ValueError("Android patch override must be an object: " + patch_id)
        logical_path = normalize_catalog_path(entry.get("logical_path"))
        source_path = normalize_catalog_path(entry.get("source_path"))
        source_root = entry.get("source_root")
        label = entry.get("label")
        source_reference = entry.get("source_reference")
        if source_root not in ("catalog", "platform"):
            raise ValueError("Android source root is invalid: " + patch_id)
        if not isinstance(label, str) or not label.strip():
            raise ValueError("Android patch override has no preset label: " + patch_id)
        if not isinstance(source_reference, str) or not source_reference.strip():
            raise ValueError("Android patch override source reference is missing: " + patch_id)
        previous = logical_paths.get(logical_path)
        current = (source_root, source_path, label)
        if previous is not None and previous != current:
            raise ValueError("Android patch IDs conflict on one logical asset path")
        logical_paths[logical_path] = current

    if len(patch_ids) != 22 or len(set(patch_ids)) != len(patch_ids):
        raise ValueError("shared Arturia active patch instance inventory changed")
    print("Arturia platform SF2 assets: PASS "
          f"(android_overrides={len(patches)}, shared_patches={len(patch_ids)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
