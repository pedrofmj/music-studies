#!/usr/bin/env python3
"""Inventory or stage the deduplicated shared Arturia SF2 distribution pack."""

from __future__ import annotations

import argparse
from datetime import date
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import sys
import tempfile
from urllib.parse import urlsplit

TOOLS_DIR = Path(__file__).resolve().parents[1]
SCRIPT_DIR = TOOLS_DIR / "benchmarks"
sys.path.insert(0, str(SCRIPT_DIR))

from arturia_shared_sf2_catalog import build_profiles, resolve_sf2_patch


DEFAULT_PATCH_CATALOG = SCRIPT_DIR / "arturia-genre-patches.json"
DEFAULT_ROUTING = SCRIPT_DIR / "arturia-shared-sf2-routing.json"
LICENSE_SCHEMA = "music-studies/arturia-sf2-licenses/v1"
INVENTORY_SCHEMA = "music-studies/arturia-sf2-pack-inventory/v1"
PACK_SCHEMA = "music-studies/arturia-sf2-pack/v1"
PLATFORM_OVERRIDE_SCHEMA = "music-studies/arturia-platform-sf2-overrides/v1"
ANDROID_RUNTIME_OVERRIDE_SCHEMA = "music-studies/arturia-android-runtime-overrides/v1"
PACK_TIMESTAMP = 315532800


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as source:
        value = json.load(source)
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def normalize_catalog_path(value: str) -> str:
    if (not isinstance(value, str) or not value or "\\" in value or
            any(ord(character) < 32 or ord(character) == 127 for character in value)):
        raise ValueError("catalog SF2 path must be a non-empty relative POSIX path")
    posix = PurePosixPath(value)
    windows = PureWindowsPath(value)
    if (posix.is_absolute() or windows.is_absolute() or windows.drive or
            any(part in ("", ".", "..") for part in posix.parts) or
            posix.as_posix() != value):
        raise ValueError("catalog SF2 path is not a safe relative path: " + value)
    return value


def resolve_source_file(source_root: Path, catalog_path: str) -> Path:
    relative = normalize_catalog_path(catalog_path)
    root = source_root.expanduser().resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f"SoundFont source root is not a directory: {source_root}")
    source = root.joinpath(*PurePosixPath(relative).parts).resolve(strict=True)
    try:
        source.relative_to(root)
    except ValueError as failure:
        raise ValueError("catalog SF2 path escapes the source root: " + relative) from failure
    if not source.is_file():
        raise ValueError(f"catalog SF2 asset is not a file: {relative}")
    return source


def validate_platform_overrides(
    overrides: dict | None,
    plugin_names: list[str],
    platform_source_root: Path | None,
) -> dict:
    if overrides is None:
        if platform_source_root is not None:
            raise ValueError("platform source root requires a platform override manifest")
        return {}
    if (overrides.get("schema") != PLATFORM_OVERRIDE_SCHEMA or
            overrides.get("platform") != "android" or
            overrides.get("source_catalog") != "arturia-genre-patches.json"):
        raise ValueError("unsupported Arturia platform-SF2 override schema")
    patches = overrides.get("patches")
    if not isinstance(patches, dict):
        raise ValueError("platform override manifest has no patches object")
    unused = sorted(set(patches) - set(plugin_names))
    if unused:
        raise ValueError("platform overrides reference inactive/unknown patches: " +
                         ", ".join(unused))

    validated = {}
    for patch_id, entry in patches.items():
        if not isinstance(entry, dict):
            raise ValueError("platform override must be an object: " + patch_id)
        logical_path = normalize_catalog_path(entry.get("logical_path"))
        source_path = normalize_catalog_path(entry.get("source_path"))
        source_root = entry.get("source_root")
        label = entry.get("label")
        source_reference = entry.get("source_reference")
        if source_root not in ("catalog", "platform"):
            raise ValueError("platform override source_root must be catalog or platform: " + patch_id)
        if source_root == "platform" and platform_source_root is None:
            raise ValueError("platform override source root is required: " + patch_id)
        if not isinstance(label, str) or not label:
            raise ValueError("platform override has no preset label: " + patch_id)
        if not isinstance(source_reference, str) or not source_reference.strip():
            raise ValueError("platform override has no source reference: " + patch_id)
        validated[patch_id] = {
            "logical_path": logical_path,
            "source_path": source_path,
            "source_root": source_root,
            "label": label,
            "source_reference": source_reference,
        }
    return validated


def hash_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as source:
        while True:
            block = source.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
            size += len(block)
    if size == 0:
        raise ValueError(f"SoundFont asset is empty: {path}")
    return digest.hexdigest(), size


def publish_directory_noreplace(staged: Path, output: Path) -> None:
    if os.name == "nt":
        os.rename(staged, output)
        return
    if sys.platform.startswith("linux"):
        library = ctypes.CDLL(None, use_errno=True)
        try:
            renameat2 = library.renameat2
        except AttributeError as failure:
            raise OSError(errno.ENOTSUP, "renameat2 is required for safe pack publication") from failure
        renameat2.argtypes = (
            ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
            ctypes.c_uint,
        )
        renameat2.restype = ctypes.c_int
        result = renameat2(
            -100, os.fsencode(staged), -100, os.fsencode(output), 1
        )
        if result == 0:
            return
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error), str(output))
    raise OSError(
        errno.ENOTSUP,
        "atomic no-replace directory publication is unsupported on this platform",
        str(output),
    )


def collect_inventory(
    source_root: Path,
    patch_catalog: dict,
    routing: dict,
    platform_overrides: dict | None = None,
    platform_source_root: Path | None = None,
) -> dict:
    plugin_names, plugin_paths, _profiles = build_profiles(patch_catalog, routing)
    root = source_root.expanduser().resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f"SoundFont source root is not a directory: {source_root}")
    platform_root = None
    if platform_source_root is not None:
        platform_root = platform_source_root.expanduser().resolve(strict=True)
        if not platform_root.is_dir():
            raise ValueError("platform SoundFont source root is not a directory")
    overrides = validate_platform_overrides(
        platform_overrides, plugin_names, platform_root
    )

    file_records = {}
    patches = []
    for patch_id, catalog_path in zip(plugin_names, plugin_paths):
        resolved_patch_id, resolved_patch = resolve_sf2_patch(
            patch_catalog, routing, patch_id
        )
        override = overrides.get(patch_id)
        if override is None:
            logical_path = normalize_catalog_path(catalog_path)
            source_path = logical_path
            source_kind = "catalog"
            label = resolved_patch["label"]
        else:
            logical_path = override["logical_path"]
            source_path = override["source_path"]
            source_kind = override["source_root"]
            label = override["label"]
        selected_root = root if source_kind == "catalog" else platform_root
        if selected_root is None:
            raise ValueError("platform source root is unavailable for " + patch_id)
        source = resolve_source_file(selected_root, source_path)
        digest, size = hash_file(source)
        record = {
            "catalog_path": logical_path,
            "source_path": source_path,
            "source_root": source_kind,
            "sha256": digest,
            "size_bytes": size,
        }
        existing = file_records.get(logical_path)
        if existing is not None and existing != record:
            raise ValueError("one logical SF2 path maps to conflicting assets: " + logical_path)
        file_records[logical_path] = record
        patches.append({
            "patch_id": patch_id,
            "resolved_patch_id": resolved_patch_id,
            "label": label,
            "catalog_path": logical_path,
        })
    inventory = {
        "schema": INVENTORY_SCHEMA,
        "source_catalog": "arturia-genre-patches.json",
        "source_routing": "arturia-shared-sf2-routing.json",
        "patches": patches,
        "files": [file_records[path] for path in sorted(file_records)],
    }
    if platform_overrides is not None:
        inventory["platform"] = "android"
    return inventory


def validate_license_manifest(license_manifest: dict, inventory: dict) -> dict:
    if license_manifest.get("schema") != LICENSE_SCHEMA:
        raise ValueError("unsupported Arturia SF2 license manifest schema")
    entries = license_manifest.get("assets")
    if not isinstance(entries, dict):
        raise ValueError("license manifest has no assets object")

    expected_paths = {entry["catalog_path"] for entry in inventory["files"]}
    supplied_paths = set(entries)
    if supplied_paths != expected_paths:
        missing = sorted(expected_paths - supplied_paths)
        extra = sorted(supplied_paths - expected_paths)
        details = []
        if missing:
            details.append("missing approvals: " + ", ".join(missing))
        if extra:
            details.append("unused approvals: " + ", ".join(extra))
        raise ValueError("license manifest does not match the pack: " + "; ".join(details))

    validated = {}
    for file_record in inventory["files"]:
        catalog_path = file_record["catalog_path"]
        entry = entries[catalog_path]
        if not isinstance(entry, dict):
            raise ValueError("license record must be an object: " + catalog_path)
        if entry.get("approved_for_distribution") is not True:
            raise ValueError("redistribution is not approved for: " + catalog_path)
        if entry.get("sha256") != file_record["sha256"]:
            raise ValueError("license approval hash differs for: " + catalog_path)
        required_text = (
            "license_id", "license_text", "attribution", "reviewed_by",
            "reviewed_on",
        )
        for field in required_text:
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                raise ValueError(f"license record needs {field}: {catalog_path}")
        source_url = entry.get("source_url")
        source_reference = entry.get("source_reference")
        if source_url is not None:
            if not isinstance(source_url, str) or not source_url.strip():
                raise ValueError("license source_url must be non-empty: " + catalog_path)
            try:
                parsed_url = urlsplit(source_url)
                hostname = parsed_url.hostname
                _port = parsed_url.port
            except ValueError as failure:
                raise ValueError("license source_url is invalid: " + catalog_path) from failure
            if (parsed_url.scheme not in ("https", "http") or not hostname or
                    parsed_url.username or parsed_url.password):
                raise ValueError("license source_url must be HTTP(S): " + catalog_path)
        elif not isinstance(source_reference, str) or not source_reference.strip():
            raise ValueError("license record needs source_url or source_reference: " +
                             catalog_path)
        try:
            review_date = date.fromisoformat(entry["reviewed_on"])
        except ValueError as failure:
            raise ValueError("license reviewed_on must use YYYY-MM-DD: " +
                             catalog_path) from failure
        if review_date.isoformat() != entry["reviewed_on"]:
            raise ValueError("license reviewed_on must use YYYY-MM-DD: " + catalog_path)
        validated[catalog_path] = entry
    return validated


def build_pack_manifest(inventory: dict, licenses: dict) -> tuple[dict, dict[str, str]]:
    files_by_hash = {}
    for record in inventory["files"]:
        files_by_hash.setdefault(record["sha256"], []).append(record)

    asset_paths = {}
    assets = []
    license_texts = {}
    for digest in sorted(files_by_hash):
        records = sorted(files_by_hash[digest], key=lambda item: item["catalog_path"])
        size = records[0]["size_bytes"]
        if any(record["size_bytes"] != size for record in records):
            raise ValueError("same SHA-256 has inconsistent file sizes")
        output_path = f"assets/{digest}.sf2"
        for record in records:
            asset_paths[record["catalog_path"]] = output_path

        asset_licenses = []
        for record in records:
            catalog_path = record["catalog_path"]
            license_entry = licenses[catalog_path]
            text = license_entry["license_text"]
            if not text.endswith("\n"):
                text += "\n"
            text_hash = hashlib.sha256(text.encode("utf-8")).hexdigest()
            notice_path = f"licenses/{digest}-{text_hash}.txt"
            license_texts[notice_path] = text
            license_record = {
                "catalog_path": catalog_path,
                "license_id": license_entry["license_id"],
                "attribution": license_entry["attribution"],
                "notice_path": notice_path,
            }
            if "source_url" in license_entry:
                license_record["source_url"] = license_entry["source_url"]
            else:
                license_record["source_reference"] = license_entry["source_reference"]
            asset_licenses.append(license_record)
        assets.append({
            "path": output_path,
            "sha256": digest,
            "size_bytes": size,
            "catalog_paths": [record["catalog_path"] for record in records],
            "licenses": asset_licenses,
        })

    patches = [
        {
            "patch_id": patch["patch_id"],
            "resolved_patch_id": patch["resolved_patch_id"],
            "label": patch["label"],
            "catalog_path": patch["catalog_path"],
            "asset_path": asset_paths[patch["catalog_path"]],
        }
        for patch in inventory["patches"]
    ]
    manifest = {
        "schema": PACK_SCHEMA,
        "source_catalog": inventory["source_catalog"],
        "source_routing": inventory["source_routing"],
        "patches": patches,
        "assets": assets,
        "total_bytes": sum(asset["size_bytes"] for asset in assets),
    }
    if "platform" in inventory:
        manifest["platform"] = inventory["platform"]
    return manifest, license_texts


def build_asset_index(manifest: dict) -> str:
    rows = []
    for asset in manifest["assets"]:
        for catalog_path in asset["catalog_paths"]:
            rows.append((
                catalog_path,
                asset["path"],
                asset["sha256"],
                str(asset["size_bytes"]),
            ))
    rows.sort(key=lambda row: row[0])
    if len({row[0] for row in rows}) != len(rows):
        raise ValueError("pack contains duplicate catalog paths")
    lines = [
        "music-studies/arturia-sf2-pack-index/v1",
        "catalog_path\tasset_path\tsha256\tsize_bytes",
    ]
    lines.extend("\t".join(row) for row in rows)
    return "\n".join(lines) + "\n"


def build_android_runtime_overrides(platform_overrides: dict) -> dict:
    return {
        "schema": ANDROID_RUNTIME_OVERRIDE_SCHEMA,
        "platform": "android",
        "source_catalog": "arturia-genre-patches.json",
        "patches": {
            patch_id: {
                "path": entry["logical_path"],
                "label": entry["label"],
            }
            for patch_id, entry in sorted(platform_overrides["patches"].items())
        },
    }


def stage_pack(
    source_root: Path,
    output: Path,
    patch_catalog: dict,
    routing: dict,
    license_manifest: dict,
    platform_overrides: dict | None = None,
    platform_source_root: Path | None = None,
) -> dict:
    inventory = collect_inventory(
        source_root, patch_catalog, routing,
        platform_overrides, platform_source_root,
    )
    licenses = validate_license_manifest(license_manifest, inventory)
    manifest, license_texts = build_pack_manifest(inventory, licenses)
    output = output.expanduser().absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f"pack output already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    lock_path = output.with_name("." + output.name + ".lock")
    try:
        lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as failure:
        raise ValueError(f"another staging operation owns the pack lock: {lock_path}") from failure

    try:
        os.close(lock_fd)
        if output.exists() or output.is_symlink():
            raise ValueError(f"pack output appeared during staging: {output}")
        source_root = source_root.expanduser().resolve(strict=True)
        if platform_source_root is not None:
            platform_source_root = platform_source_root.expanduser().resolve(strict=True)
        with tempfile.TemporaryDirectory(
            prefix=".arturia-sf2-pack-", dir=output.parent
        ) as temporary:
            staged = Path(temporary) / "pack"
            (staged / "assets").mkdir(parents=True)
            (staged / "licenses").mkdir()
            copied_hashes = set()
            for record in inventory["files"]:
                digest = record["sha256"]
                if digest in copied_hashes:
                    continue
                asset_root = (
                    source_root if record["source_root"] == "catalog"
                    else platform_source_root
                )
                if asset_root is None:
                    raise ValueError("platform SF2 source root is unavailable")
                source = resolve_source_file(asset_root, record["source_path"])
                destination = staged / "assets" / f"{digest}.sf2"
                shutil.copyfile(source, destination)
                copied_hash, copied_size = hash_file(destination)
                if copied_hash != digest or copied_size != record["size_bytes"]:
                    raise ValueError("SoundFont changed while staging: " + record["catalog_path"])
                copied_hashes.add(digest)

            for relative_path, text in license_texts.items():
                (staged / relative_path).write_text(text, encoding="utf-8", newline="\n")
            (staged / "manifest.json").write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            (staged / "asset-index.tsv").write_text(
                build_asset_index(manifest),
                encoding="utf-8",
                newline="\n",
            )
            if platform_overrides is not None:
                runtime_overrides = build_android_runtime_overrides(platform_overrides)
                (staged / "android-catalog-overrides.json").write_text(
                    json.dumps(runtime_overrides, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8",
                    newline="\n",
                )
            for path in sorted(staged.rglob("*"), reverse=True):
                os.chmod(path, 0o755 if path.is_dir() else 0o644)
                os.utime(path, (PACK_TIMESTAMP, PACK_TIMESTAMP))
            os.chmod(staged, 0o755)
            os.utime(staged, (PACK_TIMESTAMP, PACK_TIMESTAMP))
            if output.exists() or output.is_symlink():
                raise ValueError(f"pack output appeared during staging: {output}")
            publish_directory_noreplace(staged, output)
    finally:
        lock_path.unlink(missing_ok=True)
    return manifest


def write_inventory(path: Path, inventory: dict) -> None:
    path = path.expanduser().absolute()
    if path.exists() or path.is_symlink():
        raise ValueError(f"inventory output already exists: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(inventory, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True,
                        help="build-time Galaxy Sound Bank root (for example Insync)")
    parser.add_argument("--catalog", type=Path, default=DEFAULT_PATCH_CATALOG)
    parser.add_argument("--routing", type=Path, default=DEFAULT_ROUTING)
    parser.add_argument("--platform-overrides", type=Path,
                        help="platform-specific patch asset and source mapping JSON")
    parser.add_argument("--platform-source-root", type=Path,
                        help="source root for platform-specific SoundFonts")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--inventory-output", type=Path,
                      help="write file hashes and sizes without packaging assets")
    mode.add_argument("--output", type=Path,
                      help="create a new self-contained pack directory")
    parser.add_argument("--license-manifest", type=Path,
                        help="per-file redistribution approvals required with --output")
    arguments = parser.parse_args(argv)

    try:
        patch_catalog = load_json(arguments.catalog)
        routing = load_json(arguments.routing)
        platform_overrides = (
            load_json(arguments.platform_overrides)
            if arguments.platform_overrides is not None else None
        )
        if arguments.inventory_output is not None:
            if arguments.license_manifest is not None:
                parser.error("--license-manifest is only valid with --output")
            inventory = collect_inventory(
                arguments.source_root, patch_catalog, routing,
                platform_overrides, arguments.platform_source_root,
            )
            write_inventory(arguments.inventory_output, inventory)
            print("Arturia SF2 inventory written: files=%d patches=%d" % (
                len(inventory["files"]), len(inventory["patches"])
            ))
            return 0
        if arguments.license_manifest is None:
            parser.error("--license-manifest is required with --output")
        manifest = stage_pack(
            arguments.source_root,
            arguments.output,
            patch_catalog,
            routing,
            load_json(arguments.license_manifest),
            platform_overrides,
            arguments.platform_source_root,
        )
        print("Arturia SF2 pack staged: files=%d patches=%d bytes=%d" % (
            len(manifest["assets"]), len(manifest["patches"]),
            manifest["total_bytes"],
        ))
        return 0
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print(f"Arturia SF2 pack failed: {error}", file=os.sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
