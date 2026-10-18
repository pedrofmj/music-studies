#!/usr/bin/env python3
"""Validate the shared-SF2 catalogue and Arturia control contracts."""

from __future__ import annotations

import json
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
TOOLS_ROOT = SCRIPT_PATH.parents[1]
REPOSITORY_ROOT = SCRIPT_PATH.parents[4]
BENCHMARKS = TOOLS_ROOT / "benchmarks"
RIG_ROOT = REPOSITORY_ROOT / "src/performance-rigs/pedro-performance-rig"


def main() -> int:
    patch_catalog = json.loads(
        (BENCHMARKS / "arturia-genre-patches.json").read_text(encoding="utf-8")
    )
    routing = json.loads(
        (BENCHMARKS / "arturia-shared-sf2-routing.json").read_text(
            encoding="utf-8"
        )
    )
    hardware = json.loads(
        (RIG_ROOT / "hardware-presets/arturia-current-rack.json").read_text(
            encoding="utf-8"
        )
    )

    if routing.get("schema") != "music-studies/arturia-shared-sf2-routing/v1":
        raise ValueError("unsupported shared-SF2 routing schema")
    if routing.get("source_catalog") != "arturia-genre-patches.json":
        raise ValueError("shared-SF2 source catalog path differs")
    if routing.get("map_channels") != list(range(1, 10)):
        raise ValueError("all nine channels must have volume/reverb maps")
    if routing.get("volume_output_cc") != 7 or routing.get("reverb_output_cc") != 91:
        raise ValueError("unexpected SF2 volume or reverb output CC")

    fader_ccs = []
    knob_ccs = []
    for control in hardware["controls"]:
        message = control["message"]
        if message["channel"] != 1 or message["type"] != "cc":
            continue
        if control["id"].startswith("fader-"):
            fader_ccs.append(message["number"])
        elif control["id"].startswith("knob-"):
            knob_ccs.append(message["number"])
    if fader_ccs != routing.get("fader_ccs"):
        raise ValueError("fader CC contract differs from the hardware preset")
    if knob_ccs != routing.get("knob_ccs"):
        raise ValueError("knob CC contract differs from the hardware preset")

    patches = patch_catalog["patches"]
    genres = patch_catalog["genres"]
    active_layers = routing.get("active_layers", {})
    substitutions = routing.get("sf2_substitutions", {})
    if set(genres) != set(active_layers):
        raise ValueError("genre and active-layer profile sets differ")
    used_patch_ids = {patch_id for profile in genres.values() for patch_id in profile}
    non_sf2_patches = {
        patch_id for patch_id in used_patch_ids
        if patches[patch_id].get("engine") != "sf2"
    }
    if not non_sf2_patches.issubset(substitutions):
        raise ValueError("a non-SF2 catalog patch has no shared-engine substitution")
    for source, target in substitutions.items():
        if source not in patches or target not in patches:
            raise ValueError(f"unknown SF2 substitution: {source} -> {target}")
        if patches[target].get("engine") != "sf2":
            raise ValueError(f"substitution target is not SF2: {source} -> {target}")

    resolved_patches = 0
    active_route_count = 0
    for genre, patch_ids in genres.items():
        layers = active_layers[genre]
        if len(patch_ids) != 9:
            raise ValueError(f"{genre} does not define nine patches")
        if not layers or len(set(layers)) != len(layers) or any(
            layer < 1 or layer > 9 for layer in layers
        ):
            raise ValueError(f"{genre} has invalid active layers")
        active_route_count += len(layers)
        for patch_id in patch_ids:
            if patch_id not in patches:
                raise ValueError(f"{genre} references unknown patch {patch_id}")
            patch = patches[patch_id]
            if patch.get("engine") != "sf2":
                patch = patches[substitutions[patch_id]]
            if patch.get("engine") != "sf2" or not patch.get("path"):
                raise ValueError(f"{genre} patch does not resolve to an SF2 asset")
            if Path(patch["path"]).is_absolute():
                raise ValueError(f"SF2 path must remain relative: {patch['path']}")
            resolved_patches += 1

    print(
        "Arturia shared-SF2 routing: PASS "
        f"(profiles={len(genres)}, active_routes={active_route_count}, "
        f"patch_slots={resolved_patches}, substitutions={len(substitutions)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
