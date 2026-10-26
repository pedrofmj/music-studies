#!/usr/bin/env python3
"""Write a Carla project copy whose SF2 references point into one installed pack."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from pathlib import PurePosixPath
import re
import sys
import tempfile
import xml.etree.ElementTree as ET
from urllib.parse import urlsplit

from package_arturia_sf2_pack import (
    PACK_SCHEMA,
    hash_file,
    load_json,
    normalize_catalog_path,
    resolve_source_file,
)


def load_verified_pack(pack_root: Path) -> tuple[Path, dict, list[dict]]:
    root = pack_root.expanduser().resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f"SF2 pack root is not a directory: {pack_root}")
    manifest = load_json(root / "manifest.json")
    if manifest.get("schema") != PACK_SCHEMA:
        raise ValueError("unsupported SF2 pack manifest schema")
    assets = manifest.get("assets")
    patches = manifest.get("patches")
    if not isinstance(assets, list) or not assets or not isinstance(patches, list) or not patches:
        raise ValueError("SF2 pack manifest has no assets or patches")

    assets_by_path = {}
    total_bytes = 0
    for asset in assets:
        if not isinstance(asset, dict):
            raise ValueError("SF2 pack asset record must be an object")
        relative = normalize_catalog_path(asset.get("path"))
        digest = asset.get("sha256")
        size = asset.get("size_bytes")
        if (not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None or
                type(size) is not int or size <= 0):
            raise ValueError("SF2 pack asset has an invalid hash or size: " + relative)
        if relative in assets_by_path:
            raise ValueError("SF2 pack has duplicate asset paths: " + relative)
        catalog_paths = asset.get("catalog_paths")
        if not isinstance(catalog_paths, list) or not catalog_paths:
            raise ValueError("SF2 pack asset has no catalog paths: " + relative)
        normalized_catalog_paths = [
            normalize_catalog_path(catalog_path) for catalog_path in catalog_paths
        ]
        if len(set(normalized_catalog_paths)) != len(normalized_catalog_paths):
            raise ValueError("SF2 pack asset has duplicate catalog paths: " + relative)
        asset["catalog_paths"] = normalized_catalog_paths
        asset_file = resolve_source_file(root, relative)
        actual_digest, actual_size = hash_file(asset_file)
        if actual_digest != digest or actual_size != size:
            raise ValueError("SF2 pack asset failed hash/size verification: " + relative)

        license_records = asset.get("licenses")
        if not isinstance(license_records, list) or not license_records:
            raise ValueError("SF2 pack asset has no license notices: " + relative)
        for license_record in license_records:
            if not isinstance(license_record, dict):
                raise ValueError("SF2 pack license record must be an object: " + relative)
            if any(not isinstance(license_record.get(field), str) or
                   not license_record[field].strip()
                   for field in ("license_id", "attribution")):
                raise ValueError("SF2 pack license metadata is incomplete: " + relative)
            source_url = license_record.get("source_url")
            source_reference = license_record.get("source_reference")
            if source_url is not None:
                if not isinstance(source_url, str) or not source_url.strip():
                    raise ValueError("SF2 pack license source URL is empty: " + relative)
                try:
                    parsed_url = urlsplit(source_url)
                    hostname = parsed_url.hostname
                    _port = parsed_url.port
                except ValueError as failure:
                    raise ValueError("SF2 pack license source URL is invalid: " +
                                     relative) from failure
                if (parsed_url.scheme not in ("http", "https") or not hostname or
                        parsed_url.username or parsed_url.password):
                    raise ValueError("SF2 pack license source URL is invalid: " + relative)
            elif not isinstance(source_reference, str) or not source_reference.strip():
                raise ValueError("SF2 pack has no license source reference: " + relative)
            notice = normalize_catalog_path(license_record.get("notice_path"))
            notice_file = resolve_source_file(root, notice)
            if not notice_file.read_text(encoding="utf-8").strip():
                raise ValueError("SF2 pack license notice is empty: " + notice)
        assets_by_path[relative] = asset
        total_bytes += size

    if manifest.get("total_bytes") != total_bytes:
        raise ValueError("SF2 pack total_bytes differs from its asset records")

    seen_patches = set()
    referenced_assets = set()
    for patch in patches:
        if not isinstance(patch, dict):
            raise ValueError("SF2 pack patch record must be an object")
        name = patch.get("patch_id")
        resolved_name = patch.get("resolved_patch_id")
        label = patch.get("label")
        if (not isinstance(name, str) or not name or name in seen_patches or
                not isinstance(resolved_name, str) or not resolved_name or
                not isinstance(label, str) or not label):
            raise ValueError("SF2 pack has an invalid or duplicate patch record")
        seen_patches.add(name)
        normalize_catalog_path(patch.get("catalog_path"))
        asset_path = normalize_catalog_path(patch.get("asset_path"))
        if asset_path not in assets_by_path:
            raise ValueError("SF2 pack patch references a missing asset: " + name)
        if patch["catalog_path"] not in assets_by_path[asset_path]["catalog_paths"]:
            raise ValueError("SF2 pack patch-to-asset mapping is inconsistent: " + name)
        referenced_assets.add(asset_path)
    if referenced_assets != set(assets_by_path):
        raise ValueError("SF2 pack contains unreferenced assets")
    return root, manifest, patches


def set_info_text(plugin: ET.Element, field: str, value: str) -> None:
    info = plugin.find("Info")
    if info is None:
        raise ValueError("Carla SF2 plugin has no Info block")
    element = info.find(field)
    if element is None:
        element = ET.SubElement(info, field)
    element.text = value


def materialize_project(template_project: Path, pack_root: Path, output: Path) -> int:
    if not template_project.is_file():
        raise ValueError(f"Carla template project does not exist: {template_project}")
    pack_root, _manifest, patches = load_verified_pack(pack_root)
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    tree = ET.parse(template_project, parser=parser)
    root = tree.getroot()
    if root.tag != "CARLA-PROJECT":
        raise ValueError("template is not a Carla project")

    plugins_by_name = {}
    for plugin in root.findall("Plugin"):
        name = plugin.findtext("Info/Name") or ""
        plugins_by_name.setdefault(name, []).append(plugin)

    for patch in patches:
        name = patch["patch_id"]
        matches = plugins_by_name.get(name, [])
        if len(matches) != 1:
            raise ValueError(
                f"Carla template must contain exactly one plugin named {name!r}"
            )
        plugin = matches[0]
        if plugin.findtext("Info/Type") != "SF2":
            raise ValueError(f"Carla plugin {name!r} is not an SF2 instance")
        asset_path = resolve_source_file(pack_root, patch["asset_path"])
        set_info_text(plugin, "Filename", str(asset_path))
        set_info_text(plugin, "Label", patch["label"])

    ET.indent(root, space=" ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    content = (
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE CARLA-PROJECT>\n"
        + body + "\n"
    )
    output = output.expanduser().absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f"Carla project output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    lock_path = output.with_name("." + output.name + ".lock")
    try:
        lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as failure:
        raise ValueError(f"another project materialization owns the lock: {lock_path}") from failure

    temporary_path = None
    try:
        os.close(lock_fd)
        if output.exists() or output.is_symlink():
            raise ValueError(f"Carla project output appeared during materialization: {output}")
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=output.parent,
            prefix="." + output.name + ".", suffix=".tmp", delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(content)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.chmod(temporary_path, 0o644)
        try:
            os.link(temporary_path, output)
        except FileExistsError as failure:
            raise ValueError(f"Carla project output appeared during materialization: {output}") from failure
        temporary_path.unlink()
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        lock_path.unlink(missing_ok=True)
    return len(patches)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template-project", type=Path, required=True,
                        help="caller-provided Carla project; audio graph is preserved")
    parser.add_argument("--pack-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="new Carla project file; existing outputs are not overwritten")
    arguments = parser.parse_args(argv)
    try:
        count = materialize_project(
            arguments.template_project, arguments.pack_root, arguments.output
        )
    except (OSError, json.JSONDecodeError, ValueError, ET.ParseError) as error:
        print(f"Carla SF2 project materialization failed: {error}", file=sys.stderr)
        return 1
    print(f"Carla SF2 project materialized: patches={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
