#!/usr/bin/env python3
"""Create and apply a journaled phase-3 MIDI patch-library migration.

This tool intentionally never deletes or deduplicates patch files. It moves
only files listed in the approved phase-2 plan and updates the six ACE Fluid
Synth references in a backed-up Carla project.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile
from typing import Any


MANIFEST_SCHEMA = "https://music-studies.local/schemas/midi-patch-library-migration/v1"
JOURNAL_SCHEMA = "https://music-studies.local/schemas/midi-patch-library-migration-journal/v1"
SOUNDFONT_KEY = "urn:ardour:a-fluidsynth:sf2file"
CARLA_SOUNDFONTS = (
    "Crystal Rhodes.sf2",
    "PAD WORSHIP 1 - TIMBRES PREMIUM.sf2",
    "PAD WORSHIP 2- TIMBRES PREMIUM.sf2",
    "AnalogPAD - TIMBRES PREMIUM.sf2",
    "Nord White Grand.sf2",
    "Drum_Set.sf2",
)
PREFERRED_CARLA_SOURCE_PACK = "Pack 53 GB Audio Evolution"


def utc_now() -> str:
    return dt.datetime.now(tz=dt.timezone.utc).isoformat().replace("+00:00", "Z")


def safe_relative(value: str, field: str) -> Path:
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise ValueError(f"Unsafe {field}: {value!r}")
    return path


def atomic_write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as temporary:
        json.dump(data, temporary, ensure_ascii=False, indent=2)
        temporary.write("\n")
        temporary_path = Path(temporary.name)
    temporary_path.replace(path)


def atomic_write_text(path: Path, text: str, mode: int) -> None:
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as temporary:
        temporary.write(text)
        temporary_path = Path(temporary.name)
    os.chmod(temporary_path, stat.S_IMODE(mode))
    temporary_path.replace(path)


def read_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def make_manifest(inventory_path: Path, output_path: Path, source_root: Path, carla_project: Path) -> None:
    inventory = read_json(inventory_path)
    if inventory.get("phase") != 2 or inventory.get("organization", {}).get("status") != "proposed":
        raise ValueError("Expected a phase-2 inventory with a proposed organization")
    moves = []
    files_by_name: dict[str, list[dict[str, Any]]] = {}
    for record in inventory.get("files", []):
        organization = record.get("organization", {})
        current = safe_relative(organization.get("current_relative_path", ""), "current_relative_path")
        proposed = safe_relative(organization.get("proposed_path", ""), "proposed_path")
        if proposed.parts[0] != "Library":
            raise ValueError(f"Target must be under Library: {proposed}")
        move = {
            "current_relative_path": current.as_posix(),
            "proposed_relative_path": proposed.as_posix(),
            "bytes": record["bytes"],
            "type": record["type"],
        }
        moves.append(move)
        files_by_name.setdefault(record["filename"], []).append(move)

    carla_references = []
    for filename in CARLA_SOUNDFONTS:
        candidates = files_by_name.get(filename, [])
        preferred = [
            candidate
            for candidate in candidates
            if candidate["current_relative_path"].split("/", 1)[0] == PREFERRED_CARLA_SOURCE_PACK
        ]
        selected = (preferred or candidates)
        if not selected:
            raise ValueError(f"Carla SoundFont is absent from the inventory: {filename}")
        selected = sorted(selected, key=lambda candidate: candidate["current_relative_path"])[0]
        carla_references.append(
            {
                "current_value": filename,
                "selected_current_relative_path": selected["current_relative_path"],
                "selected_target_relative_path": selected["proposed_relative_path"],
                "bytes": selected["bytes"],
            }
        )

    manifest = {
        "schema": MANIFEST_SCHEMA,
        "created_at": utc_now(),
        "source_root": str(source_root),
        "carla_project": str(carla_project),
        "behavior": {
            "moves_only": True,
            "deletes_files": False,
            "deduplicates_files": False,
            "creates_project_backup": True,
            "updates_ace_fluidsynth_paths": True,
        },
        "moves": sorted(moves, key=lambda item: item["current_relative_path"]),
        "carla_references": carla_references,
    }
    atomic_write_json(output_path, manifest)
    print(json.dumps({"moves": len(moves), "carla_references": len(carla_references)}, sort_keys=True))
    print(f"Wrote {output_path}")


def resolve_paths(manifest: dict[str, Any], move: dict[str, Any]) -> tuple[Path, Path]:
    root = Path(manifest["source_root"])
    source = root / safe_relative(move["current_relative_path"], "current_relative_path")
    target = root / safe_relative(move["proposed_relative_path"], "proposed_relative_path")
    return source, target


def preflight(manifest: dict[str, Any], project: Path) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    summary = {"ready_to_move": 0, "already_moved": 0, "conflicts": 0, "missing": 0}
    root = Path(manifest["source_root"])
    if not root.is_dir():
        errors.append(f"Source root is unavailable: {root}")
    for move in manifest["moves"]:
        source, target = resolve_paths(manifest, move)
        source_exists = source.is_file()
        target_exists = target.is_file()
        if source_exists and target_exists:
            summary["conflicts"] += 1
            errors.append(f"Both source and target exist: {source} -> {target}")
        elif source_exists:
            if source.stat().st_size != move["bytes"]:
                errors.append(f"Source size changed since inventory: {source}")
            else:
                summary["ready_to_move"] += 1
        elif target_exists:
            if target.stat().st_size != move["bytes"]:
                errors.append(f"Target size does not match manifest: {target}")
            else:
                summary["already_moved"] += 1
        else:
            summary["missing"] += 1
            errors.append(f"Neither source nor target exists: {source}")

    if not project.is_file():
        errors.append(f"Carla project is unavailable: {project}")
        return errors, summary
    project_text = project.read_text(encoding="utf-8")
    for reference in manifest["carla_references"]:
        pattern = re.compile(
            rf"<Key>{re.escape(SOUNDFONT_KEY)}</Key>\s*<Value>{re.escape(reference['current_value'])}</Value>"
        )
        count = len(pattern.findall(project_text))
        if count != 1:
            errors.append(
                f"Expected one Carla reference for {reference['current_value']!r}; found {count}"
            )
    return errors, summary


def update_carla_project(manifest: dict[str, Any], project: Path, backup: Path) -> list[dict[str, str]]:
    original_text = project.read_text(encoding="utf-8")
    if not backup.is_file():
        raise RuntimeError(f"Carla project backup is unavailable: {backup}")
    updated_text = original_text
    replacements = []
    root = Path(manifest["source_root"])
    for reference in manifest["carla_references"]:
        target = root / safe_relative(reference["selected_target_relative_path"], "selected_target_relative_path")
        if not target.is_file() or target.stat().st_size != reference["bytes"]:
            raise RuntimeError(f"Carla target is unavailable or has an unexpected size: {target}")
        target_value = str(target)
        pattern = re.compile(
            rf"(?P<prefix><Key>{re.escape(SOUNDFONT_KEY)}</Key>\s*<Value>)"
            rf"{re.escape(reference['current_value'])}(?P<suffix></Value>)"
        )
        updated_text, count = pattern.subn(rf"\g<prefix>{target_value}\g<suffix>", updated_text, count=1)
        if count != 1:
            raise RuntimeError(f"Failed to update exactly one Carla reference: {reference['current_value']}")
        replacements.append({"from": reference["current_value"], "to": target_value})
    if updated_text == original_text:
        raise RuntimeError("Carla project did not change")
    atomic_write_text(project, updated_text, project.stat().st_mode)
    return replacements


def apply_manifest(manifest_path: Path, journal_path: Path, execute: bool) -> int:
    manifest = read_json(manifest_path)
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise ValueError("Unsupported migration manifest")
    project = Path(manifest["carla_project"])
    errors, summary = preflight(manifest, project)
    print(json.dumps({"preflight": summary, "errors": errors}, ensure_ascii=False, indent=2))
    if errors:
        return 2
    if not execute:
        return 0

    timestamp = dt.datetime.now(tz=dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup = project.with_name(f"{project.name}.before-patch-library-{timestamp}")
    if backup.exists():
        raise RuntimeError(f"Refusing to overwrite existing Carla project backup: {backup}")
    shutil.copy2(project, backup)
    journal: dict[str, Any] = {
        "schema": JOURNAL_SCHEMA,
        "started_at": utc_now(),
        "manifest": str(manifest_path),
        "source_root": manifest["source_root"],
        "carla_project": str(project),
        "carla_backup": str(backup),
        "status": "moving_files",
        "moves": [],
        "carla_replacements": [],
    }
    atomic_write_json(journal_path, journal)
    for move in manifest["moves"]:
        source, target = resolve_paths(manifest, move)
        if target.is_file() and not source.exists():
            journal["moves"].append({"from": str(source), "to": str(target), "status": "already_moved"})
            continue
        if not source.is_file() or target.exists():
            raise RuntimeError(f"Unexpected state during move: {source} -> {target}")
        target.parent.mkdir(parents=True, exist_ok=True)
        os.replace(source, target)
        journal["moves"].append({"from": str(source), "to": str(target), "status": "moved"})
        atomic_write_json(journal_path, journal)

    journal["status"] = "updating_carla_project"
    atomic_write_json(journal_path, journal)
    journal["carla_replacements"] = update_carla_project(manifest, project, backup)
    journal["status"] = "complete"
    journal["completed_at"] = utc_now()
    atomic_write_json(journal_path, journal)
    print(json.dumps({"journal": str(journal_path), "backup": str(backup), "moves": len(journal["moves"])}, ensure_ascii=False))
    return 0


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    manifest = commands.add_parser("create-manifest", help="Derive a compact migration manifest from phase-2 inventory")
    manifest.add_argument("inventory", type=Path)
    manifest.add_argument("output", type=Path)
    manifest.add_argument("--source-root", type=Path, required=True)
    manifest.add_argument("--carla-project", type=Path, required=True)
    apply = commands.add_parser("apply", help="Preflight or apply a migration manifest")
    apply.add_argument("manifest", type=Path)
    apply.add_argument("--execute", action="store_true", help="Perform moves and update Carla; omit for read-only preflight")
    apply.add_argument("--journal", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    if args.command == "create-manifest":
        make_manifest(args.inventory, args.output, args.source_root, args.carla_project)
        return 0
    return apply_manifest(args.manifest, args.journal, args.execute)


if __name__ == "__main__":
    raise SystemExit(main())
