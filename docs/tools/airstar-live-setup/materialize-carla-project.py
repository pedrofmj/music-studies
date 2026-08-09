#!/usr/bin/env python3
"""Materialize the versioned Carla project for a target Linux home."""

from __future__ import annotations

import argparse
import os
import shutil
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


REFERENCE_HOME = Path("/home/ldap/pedro.ferreira")
REFERENCE_SOUNDFONT_ROOT = (
    REFERENCE_HOME / "Flash/PED/MIDI/Pack de Timbres/Library"
)


def replace_prefix(value: str, old: Path, new: Path) -> str:
    old_text = str(old)
    if value == old_text:
        return str(new)
    if value.startswith(old_text + os.sep):
        return str(new) + value[len(old_text) :]
    return value


def materialize(
    source: Path,
    home: Path,
    soundfont_root: Path,
) -> tuple[ET.ElementTree, int]:
    tree = ET.parse(source)
    root = tree.getroot()
    replacements = 0

    for tag in ("Binary", "Filename"):
        for element in root.iter(tag):
            if not element.text:
                continue
            original = element.text
            updated = replace_prefix(
                original, REFERENCE_SOUNDFONT_ROOT, soundfont_root
            )
            updated = replace_prefix(updated, REFERENCE_HOME, home)
            if updated != original:
                element.text = updated
                replacements += 1

    return tree, replacements


def validate(root: ET.Element, home: Path, soundfont_root: Path) -> None:
    plugins = root.findall("Plugin")
    connections = root.findall("ExternalPatchbay/Connection")
    names = [plugin.findtext("Info/Name") or "" for plugin in plugins]
    if len(plugins) != 47 or len(names) != len(set(names)):
        raise ValueError("expected 47 uniquely named plugins")
    if len(connections) != 106:
        raise ValueError(
            f"expected 106 project connections, found {len(connections)}"
        )

    missing = []
    for tag in ("Binary", "Filename"):
        for element in root.iter(tag):
            if element.text and not Path(element.text).is_file():
                missing.append(element.text)
    if missing:
        raise ValueError("missing project assets:\n" + "\n".join(missing))

    expected_roots = (str(home), str(soundfont_root))
    for element in root.iter("Filename"):
        if element.text and not element.text.startswith(expected_roots):
            raise ValueError(f"unexpected asset root: {element.text}")


def write_project(tree: ET.ElementTree, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree, space=" ")
    body = ET.tostring(
        tree.getroot(), encoding="unicode", short_empty_elements=True
    )
    content = (
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE CARLA-PROJECT>\n"
        f"{body}\n"
    )
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=target.parent, delete=False
    ) as handle:
        handle.write(content)
        temporary = Path(handle.name)
    os.chmod(temporary, 0o644)
    os.replace(temporary, target)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--home", type=Path, default=Path.home())
    parser.add_argument(
        "--soundfont-root",
        type=Path,
        default=Path.home() / "Flash/PED/MIDI/Pack de Timbres/Library",
    )
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()

    tree, replacements = materialize(
        args.source, args.home, args.soundfont_root
    )
    validate(tree.getroot(), args.home, args.soundfont_root)
    if args.check_only:
        print(
            f"OK: 47 plugins, 106 connections, {replacements} path replacements"
        )
        return 0

    if args.source.resolve() == args.target.resolve():
        raise ValueError("source and target must be different files")
    if args.target.exists():
        backup = args.target.with_suffix(args.target.suffix + ".before-install")
        shutil.copy2(args.target, backup)
        print(f"Backup: {backup}")
    write_project(tree, args.target)
    print(f"Installed Carla project: {args.target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
