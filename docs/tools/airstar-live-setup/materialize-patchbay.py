#!/usr/bin/env python3
"""Relocate saved rack output links to a target PipeWire default sink."""

from __future__ import annotations

import argparse
import json
import os
import tempfile
from pathlib import Path
from typing import Any


def find_port(graph: dict[str, Any], node_name: str, port_name: str) -> dict[str, Any]:
    matches = [
        port
        for port in graph.get("ports", [])
        if port.get("node_name") == node_name
        and port.get("name") == port_name
        and port.get("direction") == "in"
    ]
    if len(matches) != 1:
        raise ValueError(
            f"expected one {node_name}:{port_name} input, found {len(matches)}"
        )
    port = matches[0]
    if not port.get("alias") or not port.get("name_selector"):
        raise ValueError(f"default sink port lacks a semantic selector: {port}")
    return port


def replace_endpoint(
    value: Any,
    old_alias: str,
    replacement: dict[str, Any],
) -> int:
    replacements = 0
    if isinstance(value, dict):
        if value.get("alias") == old_alias:
            value["alias"] = replacement["alias"]
            value["name_selector"] = replacement["name_selector"]
            replacements += 1
        for child in value.values():
            replacements += replace_endpoint(child, old_alias, replacement)
    elif isinstance(value, list):
        for child in value:
            replacements += replace_endpoint(child, old_alias, replacement)
    return replacements


def write_json(document: dict[str, Any], target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=target.parent, delete=False
    ) as handle:
        json.dump(document, handle, indent=2, ensure_ascii=True)
        handle.write("\n")
        temporary = Path(handle.name)
    os.chmod(temporary, 0o644)
    os.replace(temporary, target)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("current_graph", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--default-node-name", required=True)
    parser.add_argument("--reference-left-alias", required=True)
    parser.add_argument("--reference-right-alias", required=True)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()

    snapshot = json.loads(args.snapshot.read_text(encoding="utf-8"))
    current = json.loads(args.current_graph.read_text(encoding="utf-8"))
    left = find_port(current, args.default_node_name, "playback_FL")
    right = find_port(current, args.default_node_name, "playback_FR")

    replacements = replace_endpoint(
        snapshot, args.reference_left_alias, left
    )
    replacements += replace_endpoint(
        snapshot, args.reference_right_alias, right
    )
    if replacements < 2:
        raise ValueError(
            f"expected at least two output endpoint replacements, found {replacements}"
        )

    if args.check_only:
        print(
            f"OK: {replacements} endpoint records -> "
            f"{left['alias']}, {right['alias']}"
        )
        return 0

    write_json(snapshot, args.target)
    print(
        f"Installed Patchbay for default sink: "
        f"{left['alias']}, {right['alias']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
