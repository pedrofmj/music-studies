#!/usr/bin/env python3
"""Exercise shared-SF2 pack inventory and staging with synthetic assets."""

from __future__ import annotations

import hashlib
import json
import tempfile
import sys
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve()
TOOLS_ROOT = SCRIPT_PATH.parents[1]
BENCHMARKS = TOOLS_ROOT / "benchmarks"
sys.path.insert(0, str(TOOLS_ROOT / "packaging"))

from package_arturia_sf2_pack import (  # noqa: E402
    PACK_TIMESTAMP,
    collect_inventory,
    normalize_catalog_path,
    stage_pack,
    validate_license_manifest,
)
from arturia_shared_sf2_catalog import build_profiles  # noqa: E402
import package_arturia_sf2_pack as packager  # noqa: E402


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def make_license_manifest(inventory: dict, hashes: dict) -> dict:
    return {
        "schema": "music-studies/arturia-sf2-licenses/v1",
        "assets": {
            path: {
                "approved_for_distribution": True,
                "sha256": hashes[path],
                "license_id": "Fixture-License",
                "license_text": "Synthetic test license notice",
                "source_url": "https://example.invalid/soundfont",
                "attribution": "Synthetic test asset",
                "reviewed_by": "offline-test",
                "reviewed_on": "2026-01-01",
            }
            for path in (record["catalog_path"] for record in inventory["files"])
        },
    }


def main() -> int:
    catalog = load_json(BENCHMARKS / "arturia-genre-patches.json")
    routing = load_json(BENCHMARKS / "arturia-shared-sf2-routing.json")
    with tempfile.TemporaryDirectory(prefix="arturia-sf2-pack-test-") as temporary:
        root = Path(temporary)
        source_root = root / "Galaxy Sound Bank"
        # Create the source tree using the catalog's exact unique active file list.
        empty_files = {}
        _names, paths, _profiles = build_profiles(catalog, routing)
        for catalog_path in dict.fromkeys(paths):
            path = source_root / catalog_path
            path.parent.mkdir(parents=True, exist_ok=True)
            content = (
                b"identical fixture bank" if len(empty_files) < 2 else
                b"fixture bank:" + catalog_path.encode("utf-8")
            )
            path.write_bytes(content)
            empty_files[catalog_path] = hashlib.sha256(path.read_bytes()).hexdigest()

        inventory = collect_inventory(source_root, catalog, routing)
        if len(inventory["files"]) != 15 or len(inventory["patches"]) != 22:
            raise AssertionError("inventory does not match the active shared-SF2 catalog")
        licenses = make_license_manifest(inventory, empty_files)
        user_attestation = json.loads(json.dumps(licenses))
        for entry in user_attestation["assets"].values():
            entry.pop("source_url")
            entry["source_reference"] = "User confirms redistribution rights for this exact hash"
        validate_license_manifest(user_attestation, inventory)
        user_pack = root / "user-attested-pack"
        user_manifest = stage_pack(
            source_root, user_pack, catalog, routing, user_attestation
        )
        if not any(
                "source_reference" in license_record and
                "source_url" not in license_record
                for asset in user_manifest["assets"]
                for license_record in asset["licenses"]):
            raise AssertionError("user approval source reference was not recorded")

        invalid = json.loads(json.dumps(licenses))
        invalid["assets"][inventory["files"][0]["catalog_path"]][
            "approved_for_distribution"
        ] = False
        try:
            validate_license_manifest(invalid, inventory)
        except ValueError as failure:
            if "not approved" not in str(failure):
                raise
        else:
            raise AssertionError("unapproved assets passed the redistribution gate")
        invalid_url = json.loads(json.dumps(licenses))
        invalid_url["assets"][inventory["files"][0]["catalog_path"]][
            "source_url"
        ] = "https://"
        try:
            validate_license_manifest(invalid_url, inventory)
        except ValueError as failure:
            if "source_url" not in str(failure):
                raise
        else:
            raise AssertionError("invalid license source URL was accepted")
        invalid_port = json.loads(json.dumps(licenses))
        invalid_port["assets"][inventory["files"][0]["catalog_path"]][
            "source_url"
        ] = "https://example.invalid:abc/soundfont"
        try:
            validate_license_manifest(invalid_port, inventory)
        except ValueError as failure:
            if "source_url" not in str(failure):
                raise
        else:
            raise AssertionError("license source URL with an invalid port was accepted")
        invalid_date = json.loads(json.dumps(licenses))
        invalid_date["assets"][inventory["files"][0]["catalog_path"]][
            "reviewed_on"
        ] = "not-a-date"
        try:
            validate_license_manifest(invalid_date, inventory)
        except ValueError as failure:
            if "reviewed_on" not in str(failure):
                raise
        else:
            raise AssertionError("invalid license review date was accepted")
        rejected_pack = root / "rejected-pack"
        try:
            stage_pack(source_root, rejected_pack, catalog, routing, invalid)
        except ValueError as failure:
            if "not approved" not in str(failure):
                raise
        else:
            raise AssertionError("stager accepted an unapproved redistribution manifest")
        if rejected_pack.exists():
            raise AssertionError("failed staging left a partial pack directory")

        first_pack = root / "pack-one"
        manifest = stage_pack(source_root, first_pack, catalog, routing, licenses)
        second_pack = root / "pack-two"
        second_manifest = stage_pack(source_root, second_pack, catalog, routing, licenses)
        first_bytes = (first_pack / "manifest.json").read_bytes()
        if first_bytes != (second_pack / "manifest.json").read_bytes():
            raise AssertionError("pack manifest is not reproducible")
        index_bytes = (first_pack / "asset-index.tsv").read_bytes()
        if index_bytes != (second_pack / "asset-index.tsv").read_bytes():
            raise AssertionError("pack asset index is not reproducible")
        if not index_bytes.startswith(
                b"music-studies/arturia-sf2-pack-index/v1\n"):
            raise AssertionError("Android asset index has an unsupported schema")
        index_lines = index_bytes.decode("utf-8").splitlines()
        if index_lines[1] != "catalog_path\tasset_path\tsha256\tsize_bytes" or \
                len(index_lines) != 17:
            raise AssertionError("Android asset index does not list all catalog paths")
        if manifest != second_manifest:
            raise AssertionError("repeated staging produced different manifests")
        for pack in (first_pack, second_pack):
            for path in [pack, *pack.rglob("*")]:
                if int(path.stat().st_mtime) != PACK_TIMESTAMP:
                    raise AssertionError("pack filesystem timestamps are not normalized")
        if len(manifest["assets"]) != 14:
            raise AssertionError("content-identical SF2 fixtures were not deduplicated")
        if len(list((first_pack / "assets").glob("*.sf2"))) != 14:
            raise AssertionError("staged payload count differs from the manifest")
        for asset in manifest["assets"]:
            for license_record in asset["licenses"]:
                if not (first_pack / license_record["notice_path"]).is_file():
                    raise AssertionError("pack omitted a required license notice")
        if len(manifest["patches"]) != 22:
            raise AssertionError("pack omitted catalog patch aliases")
        if str(source_root) in first_bytes.decode("utf-8"):
            raise AssertionError("pack manifest leaked the build-time source root")
        for asset in manifest["assets"]:
            path = first_pack / asset["path"]
            if hashlib.sha256(path.read_bytes()).hexdigest() != asset["sha256"]:
                raise AssertionError("staged asset does not match the pack manifest")

        wrong_hash = json.loads(json.dumps(licenses))
        wrong_hash["assets"][inventory["files"][0]["catalog_path"]]["sha256"] = "0" * 64
        try:
            validate_license_manifest(wrong_hash, inventory)
        except ValueError as failure:
            if "hash differs" not in str(failure):
                raise
        else:
            raise AssertionError("license approval for a different file hash was accepted")

        for unsafe_path in ("../outside.sf2", "C:/outside.sf2", "folder/../outside.sf2"):
            try:
                normalize_catalog_path(unsafe_path)
            except ValueError:
                continue
            raise AssertionError(f"unsafe catalog path was accepted: {unsafe_path}")

        invalid_routing = json.loads(json.dumps(routing))
        invalid_routing["active_layers"]["worship-piano"][0] = True
        try:
            build_profiles(catalog, invalid_routing)
        except ValueError as failure:
            if "active layers" not in str(failure):
                raise
        else:
            raise AssertionError("boolean was accepted as an active layer number")

        existing_output = first_pack
        try:
            stage_pack(source_root, existing_output, catalog, routing, licenses)
        except ValueError as failure:
            if "already exists" not in str(failure):
                raise
        else:
            raise AssertionError("stager overwrote an existing pack")
        locked_output = root / "locked-pack"
        lock_path = root / ".locked-pack.lock"
        lock_path.write_text("other staging operation\n", encoding="utf-8")
        try:
            stage_pack(source_root, locked_output, catalog, routing, licenses)
        except ValueError as failure:
            if "owns the pack lock" not in str(failure):
                raise
        else:
            raise AssertionError("stager ignored another operation's output lock")
        if locked_output.exists():
            raise AssertionError("locked staging created a pack directory")

        collision_output = root / "collision-pack"
        publish = packager.publish_directory_noreplace
        collision_marker = collision_output / "created-concurrently"

        def create_collision(staged: Path, output: Path) -> None:
            output.mkdir()
            collision_marker.write_text("preserve me\n", encoding="utf-8")
            publish(staged, output)

        packager.publish_directory_noreplace = create_collision
        try:
            stage_pack(source_root, collision_output, catalog, routing, licenses)
        except FileExistsError:
            pass
        else:
            raise AssertionError("concurrent pack output was replaced")
        finally:
            packager.publish_directory_noreplace = publish
        if collision_marker.read_text(encoding="utf-8") != "preserve me\n":
            raise AssertionError("concurrently created output was overwritten")

        android_overrides = load_json(
            BENCHMARKS / "arturia-sf2-assets.android.json"
        )
        for override in android_overrides["patches"].values():
            if override["source_root"] == "catalog":
                candidate = source_root / override["source_path"]
                candidate.parent.mkdir(parents=True, exist_ok=True)
                candidate.write_bytes(
                    b"android candidate:" + override["source_path"].encode("utf-8")
                )
        android_inventory = collect_inventory(
            source_root, catalog, routing, android_overrides
        )
        if len(android_inventory["files"]) != 15 or \
                len(android_inventory["patches"]) != 22:
            raise AssertionError("Android overrides changed shared profile topology")
        android_hashes = {
            record["catalog_path"]: record["sha256"]
            for record in android_inventory["files"]
        }
        android_licenses = make_license_manifest(android_inventory, android_hashes)
        android_pack = root / "android-pack"
        android_manifest = stage_pack(
            source_root, android_pack, catalog, routing, android_licenses,
            android_overrides,
        )
        if android_manifest.get("platform") != "android":
            raise AssertionError("Android pack lost its platform identity")
        runtime_overrides = load_json(android_pack / "android-catalog-overrides.json")
        if runtime_overrides["patches"]["piano-cfx"]["label"] != "CFX Premium" or \
                runtime_overrides["patches"]["piano-jf-worship"]["label"] != "Soft Grand Piano":
            raise AssertionError("Android pack did not retain platform preset labels")
        if "source_path" in runtime_overrides["patches"]["piano-cfx"]:
            raise AssertionError("Android runtime override leaked a build source path")
        android_index = (android_pack / "asset-index.tsv").read_text(encoding="utf-8")
        if ("android-assets/cfx-premium.sf2\t" not in android_index or
                "android-assets/jf-soft-grand.sf2\t" not in android_index):
            raise AssertionError("Android asset index omitted a compact piano mapping")

        print(
            "Arturia SF2 pack staging: PASS "
            "(source_files=15, deduplicated_files=14, patches=22)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
