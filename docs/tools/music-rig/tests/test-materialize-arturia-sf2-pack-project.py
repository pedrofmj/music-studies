#!/usr/bin/env python3
"""Verify pack-backed Carla project materialization with synthetic SF2 assets."""

from __future__ import annotations

import hashlib
import json
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
TOOLS_ROOT = SCRIPT_PATH.parents[1]
BENCHMARKS = TOOLS_ROOT / "benchmarks"
PACKAGING = TOOLS_ROOT / "packaging"
sys.path.insert(0, str(PACKAGING))

from materialize_arturia_sf2_pack_project import materialize_project  # noqa: E402
from package_arturia_sf2_pack import collect_inventory, stage_pack  # noqa: E402
from arturia_shared_sf2_catalog import build_profiles  # noqa: E402


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def make_template(path: Path, patches: list[dict], omit_patch: str | None = None) -> None:
    root = ET.Element("CARLA-PROJECT", {"VERSION": "2.5"})
    ET.SubElement(root, "EngineSettings")
    for patch in patches:
        if patch["patch_id"] == omit_patch:
            continue
        plugin = ET.SubElement(root, "Plugin")
        info = ET.SubElement(plugin, "Info")
        ET.SubElement(info, "Type").text = "SF2"
        ET.SubElement(info, "Name").text = patch["patch_id"]
        ET.SubElement(info, "Filename").text = "old-soundfont.sf2"
        ET.SubElement(info, "Label").text = "old label"
        ET.SubElement(plugin, "Data")
    root.append(ET.Comment("preserve caller audio topology"))
    patchbay = ET.SubElement(root, "ExternalPatchbay")
    connection = ET.SubElement(patchbay, "Connection")
    ET.SubElement(connection, "Source").text = "SF2:output_1"
    ET.SubElement(connection, "Target").text = "Windows Audio:output_1"
    ET.indent(root, space=" ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    path.write_text(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE CARLA-PROJECT>\n" + body + "\n",
        encoding="utf-8",
    )


def main() -> int:
    catalog = load_json(BENCHMARKS / "arturia-genre-patches.json")
    routing = load_json(BENCHMARKS / "arturia-shared-sf2-routing.json")
    with tempfile.TemporaryDirectory(prefix="arturia-sf2-project-test-") as temporary:
        root = Path(temporary)
        source_root = root / "Galaxy Sound Bank"
        _names, paths, _profiles = build_profiles(catalog, routing)
        source_hashes = {}
        for catalog_path in dict.fromkeys(paths):
            source = source_root / catalog_path
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b"synthetic sf2:" + catalog_path.encode("utf-8"))
            source_hashes[catalog_path] = hashlib.sha256(source.read_bytes()).hexdigest()

        inventory = collect_inventory(source_root, catalog, routing)
        licenses = {
            "schema": "music-studies/arturia-sf2-licenses/v1",
            "assets": {
                asset["catalog_path"]: {
                    "approved_for_distribution": True,
                    "sha256": source_hashes[asset["catalog_path"]],
                    "license_id": "Synthetic-Test-License",
                    "license_text": "Synthetic notice",
                    "source_reference": "User-confirmed fixture rights",
                    "attribution": "Synthetic test asset",
                    "reviewed_by": "offline-test",
                    "reviewed_on": "2026-01-01",
                }
                for asset in inventory["files"]
            },
        }
        pack_root = root / "arturia-sf2-pack"
        manifest = stage_pack(source_root, pack_root, catalog, routing, licenses)
        template = root / "template.carxp"
        make_template(template, manifest["patches"])
        source_project_bytes = template.read_bytes()
        source_connections = [
            (connection.findtext("Source"), connection.findtext("Target"))
            for connection in ET.parse(template).getroot().findall(".//Connection")
        ]
        output = root / "materialized.carxp"
        count = materialize_project(template, pack_root, output)
        if count != 22:
            raise AssertionError(f"materialized {count} patches, expected 22")
        if template.read_bytes() != source_project_bytes:
            raise AssertionError("materializer modified its source Carla project")

        result_text = output.read_text(encoding="utf-8")
        result = ET.parse(output).getroot()
        result_connections = [
            (connection.findtext("Source"), connection.findtext("Target"))
            for connection in result.findall(".//Connection")
        ]
        if result_connections != source_connections:
            raise AssertionError("materializer changed caller-owned Carla audio links")
        if "preserve caller audio topology" not in result_text:
            raise AssertionError("materializer dropped a Carla project comment")
        plugins = {
            plugin.findtext("Info/Name"): plugin
            for plugin in result.findall("Plugin")
        }
        for patch in manifest["patches"]:
            plugin = plugins.get(patch["patch_id"])
            if plugin is None:
                raise AssertionError(f"materialized project omitted {patch['patch_id']}")
            expected_asset = (pack_root / patch["asset_path"]).resolve()
            if Path(plugin.findtext("Info/Filename") or "") != expected_asset:
                raise AssertionError(f"{patch['patch_id']} does not use its packed asset")
            if plugin.findtext("Info/Label") != patch["label"]:
                raise AssertionError(f"{patch['patch_id']} has the wrong SF2 label")

        bad_mapping_pack = root / "bad-mapping-pack"
        shutil.copytree(pack_root, bad_mapping_pack)
        mapping_manifest_path = bad_mapping_pack / "manifest.json"
        mapping_manifest = load_json(mapping_manifest_path)
        first_patch = mapping_manifest["patches"][0]
        second_patch = next(
            patch for patch in mapping_manifest["patches"][1:]
            if patch["catalog_path"] != first_patch["catalog_path"] and
            patch["asset_path"] != first_patch["asset_path"]
        )
        first_patch["asset_path"], second_patch["asset_path"] = (
            second_patch["asset_path"], first_patch["asset_path"]
        )
        mapping_manifest_path.write_text(
            json.dumps(mapping_manifest, indent=2) + "\n", encoding="utf-8"
        )
        mapping_output = root / "mapping-output.carxp"
        try:
            materialize_project(template, bad_mapping_pack, mapping_output)
        except ValueError as failure:
            if "patch-to-asset mapping" not in str(failure):
                raise
        else:
            raise AssertionError("materializer accepted swapped patch assets")
        if mapping_output.exists():
            raise AssertionError("invalid mapping produced a Carla project")

        missing_template = root / "missing-patch.carxp"
        make_template(missing_template, manifest["patches"], manifest["patches"][-1]["patch_id"])
        missing_output = root / "missing-output.carxp"
        try:
            materialize_project(missing_template, pack_root, missing_output)
        except ValueError as failure:
            if "exactly one plugin" not in str(failure):
                raise
        else:
            raise AssertionError("materializer accepted a project missing a patch")
        if missing_output.exists():
            raise AssertionError("failed project materialization left an output file")

        bad_pack = root / "bad-pack"
        shutil.copytree(pack_root, bad_pack)
        bad_manifest = load_json(bad_pack / "manifest.json")
        bad_asset = bad_pack / bad_manifest["assets"][0]["path"]
        bad_asset.write_bytes(bad_asset.read_bytes() + b"corrupt")
        corrupt_output = root / "corrupt-output.carxp"
        try:
            materialize_project(template, bad_pack, corrupt_output)
        except ValueError as failure:
            if "hash/size verification" not in str(failure):
                raise
        else:
            raise AssertionError("materializer accepted a corrupt packed asset")
        if corrupt_output.exists():
            raise AssertionError("corrupt pack produced a Carla project")

        output.write_text("do not overwrite\n", encoding="utf-8")
        try:
            materialize_project(template, pack_root, output)
        except ValueError as failure:
            if "already exists" not in str(failure):
                raise
        else:
            raise AssertionError("materializer overwrote an existing project")
        if output.read_text(encoding="utf-8") != "do not overwrite\n":
            raise AssertionError("existing project contents changed")

    print("Arturia SF2 Carla project materialization: PASS (patches=22, links=1)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
