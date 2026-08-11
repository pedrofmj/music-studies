#!/usr/bin/env python3
"""Render a compiled Performance Rig into an isolated temporary bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Mapping, Optional, Tuple


sys.dont_write_bytecode = True


SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[3]
DEFAULT_RIG_ROOT = (
    REPOSITORY_ROOT / "src" / "performance-rigs" / "pedro-performance-rig"
)
COMPILED_SCHEMA = "music-studies/compiled-performance-rig/v1"
MATERIALIZATION_SCHEMA = "music-studies/materialized-performance-rig/v1"
SUPPORTED_RIG = "pedro-performance-rig"
SUPPORTED_RIG_PROFILE = "full-live-rack"
SUPPORTED_BINDING = "airstar-current"
REFERENCE_HOME = "/home/ldap/pedro.ferreira"
REFERENCE_SOUNDFONT_ROOT = (
    REFERENCE_HOME + "/Flash/PED/MIDI/Pack de Timbres/Library"
)
REFERENCE_LEFT_ALIAS = "Speaker + Headphones:playback_FL"
REFERENCE_RIGHT_ALIAS = "Speaker + Headphones:playback_FR"


class MaterializationError(Exception):
    """A deterministic materialization-policy or input failure."""


def canonical_json_bytes(document: Any) -> bytes:
    return json.dumps(
        document,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")


def rendered_json_bytes(document: Any) -> bytes:
    return (
        json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True)
        + "\n"
    ).encode("utf-8")


def fingerprint(document: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_json_bytes(document)).hexdigest()


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise MaterializationError(message)


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def is_descendant(path: Path, parent: Path) -> bool:
    try:
        relative = path.relative_to(parent)
    except ValueError:
        return False
    return bool(relative.parts)


def resolve_relative_source(root: Path, relative_path: str, label: str) -> Path:
    require(
        PurePosixPath(relative_path).is_absolute() is False,
        f"{label} path must be relative: {relative_path!r}",
    )
    require(
        ".." not in PurePosixPath(relative_path).parts,
        f"{label} path escapes its root: {relative_path!r}",
    )
    candidate = (root / Path(*PurePosixPath(relative_path).parts)).resolve()
    require(
        is_descendant(candidate, root.resolve()),
        f"{label} path escapes its root: {relative_path!r}",
    )
    require(candidate.is_file(), f"{label} file does not exist: {candidate}")
    return candidate


def verify_compiled_definition(
    definition: Mapping[str, Any],
    rig_root: Path,
) -> Mapping[str, Any]:
    require(
        definition.get("schema") == COMPILED_SCHEMA,
        f"unsupported compiled schema: {definition.get('schema')!r}",
    )
    require(
        definition.get("rig") == SUPPORTED_RIG,
        f"unsupported Rig: {definition.get('rig')!r}",
    )
    require(
        definition.get("active_rig_profile") == SUPPORTED_RIG_PROFILE,
        "only the compiled full-live-rack profile can be materialized",
    )

    recorded_fingerprint = definition.get("definition_fingerprint")
    unsigned_definition = dict(definition)
    unsigned_definition.pop("definition_fingerprint", None)
    require(
        recorded_fingerprint == fingerprint(unsigned_definition),
        "compiled definition fingerprint mismatch",
    )

    safety = definition.get("safety", {})
    require(
        safety.get("activation") == "authoring-only"
        and safety.get("materializes_runtime") is False
        and safety.get("applies_graph_delta") is False,
        "compiled definition is not authoring-only",
    )
    graph_delta = definition.get("graph_delta", {})
    require(
        graph_delta.get("empty") is True
        and graph_delta.get("applied") is False,
        "materialization requires an empty, unapplied graph delta",
    )
    counts = graph_delta.get("counts", {})
    expected_count_keys = {
        "created_links",
        "removed_links",
        "created_objects",
        "removed_objects",
        "metadata_changes",
    }
    require(
        set(counts) == expected_count_keys
        and all(value == 0 for value in counts.values()),
        "materialization requires zero graph-delta operation counts",
    )
    operations = graph_delta.get("operations", {})
    expected_operation_keys = {
        "create_links",
        "remove_links",
        "create_objects",
        "remove_objects",
        "metadata_changes",
    }
    require(
        set(operations) == expected_operation_keys
        and all(value == [] for value in operations.values()),
        "materialization requires zero graph-delta operations",
    )

    binding_summary = definition.get("platform_binding", {})
    require(
        binding_summary.get("id") == SUPPORTED_BINDING
        and binding_summary.get("platform") == "linux",
        "only the compiled airstar-current Linux binding is supported",
    )
    manifest = definition.get("source_manifest", {}).get("platform_binding", {})
    binding_path = resolve_relative_source(
        rig_root,
        manifest.get("path", ""),
        "Platform Binding",
    )
    binding = load_json(binding_path)
    binding_fingerprint = fingerprint(binding)
    require(
        manifest.get("fingerprint") == binding_fingerprint
        and definition.get("fingerprints", {}).get("platform_binding")
        == binding_fingerprint,
        "Platform Binding fingerprint mismatch",
    )
    require(
        binding.get("id") == binding_summary.get("id")
        and binding.get("platform") == binding_summary.get("platform")
        and binding.get("rig") == definition.get("rig"),
        "compiled Platform Binding identity mismatch",
    )
    binding_safety = binding.get("safety", {})
    require(
        binding_safety.get("activation") == "authoring-only"
        and binding_safety.get("mutates_runtime") is False,
        "Platform Binding is not authoring-only",
    )

    target_bindings = definition.get("target_bindings", {})
    adapters = {
        value.get("adapter")
        for value in target_bindings.values()
        if isinstance(value, dict)
    }
    require(
        "linux.carla-project" in adapters,
        "compiled definition has no selected Carla target",
    )
    require(
        "linux.pipewire-patchbay" in adapters,
        "compiled definition has no selected Patchbay target",
    )
    return binding


def verified_repository_source(
    binding: Mapping[str, Any],
    path_id: str,
    repository_root: Path,
) -> Path:
    entry = binding.get("paths", {}).get(path_id, {})
    require(
        entry.get("resolver") == "repository-relative",
        f"{path_id} must use the repository-relative resolver",
    )
    path = resolve_relative_source(
        repository_root,
        entry.get("value", ""),
        path_id,
    )
    require(
        entry.get("sha256") == file_sha256(path),
        f"{path_id} checksum mismatch",
    )
    return path


def validate_posix_root(value: str, label: str) -> str:
    path = PurePosixPath(value)
    require(path.is_absolute(), f"{label} must be an absolute POSIX path")
    require(".." not in path.parts, f"{label} must not contain '..'")
    require("\\" not in value, f"{label} must use POSIX separators")
    normalized = str(path)
    require(normalized != "/", f"{label} must not be the filesystem root")
    return normalized


def replace_posix_prefix(value: str, old: str, new: str) -> str:
    if value == old:
        return new
    if value.startswith(old + "/"):
        return new + value[len(old) :]
    return value


def render_carla_project(
    source: Path,
    target_home: str,
    target_soundfont_root: str,
) -> Tuple[bytes, int]:
    tree = ET.parse(source)
    root = tree.getroot()
    replacements = 0
    relocatable_paths = 0
    for tag in ("Binary", "Filename"):
        for element in root.iter(tag):
            if not element.text:
                continue
            original = element.text
            if original == REFERENCE_HOME or original.startswith(
                REFERENCE_HOME + "/"
            ):
                relocatable_paths += 1
            updated = replace_posix_prefix(
                original,
                REFERENCE_SOUNDFONT_ROOT,
                target_soundfont_root,
            )
            updated = replace_posix_prefix(updated, REFERENCE_HOME, target_home)
            if updated != original:
                element.text = updated
                replacements += 1
    require(relocatable_paths > 0, "Carla source contained no relocatable paths")
    ET.indent(tree, space=" ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    content = (
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE CARLA-PROJECT>\n"
        f"{body}\n"
    )
    return content.encode("utf-8"), replacements


def find_sink_port(
    graph: Mapping[str, Any],
    node_name: str,
    port_name: str,
) -> Mapping[str, Any]:
    matches = [
        port
        for port in graph.get("ports", [])
        if port.get("node_name") == node_name
        and port.get("name") == port_name
        and port.get("direction") == "in"
    ]
    require(
        len(matches) == 1,
        f"expected one {node_name}:{port_name} input, found {len(matches)}",
    )
    port = matches[0]
    require(
        bool(port.get("alias")) and bool(port.get("name_selector")),
        f"{node_name}:{port_name} lacks a semantic selector",
    )
    return {
        "alias": port["alias"],
        "name_selector": port["name_selector"],
    }


def replace_endpoint(value: Any, old_alias: str, replacement: Mapping[str, Any]) -> int:
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


def render_patchbay(
    source: Path,
    current_graph: Mapping[str, Any],
    default_node_name: str,
) -> Tuple[bytes, int, Mapping[str, Any], Mapping[str, Any]]:
    document = load_json(source)
    left = find_sink_port(current_graph, default_node_name, "playback_FL")
    right = find_sink_port(current_graph, default_node_name, "playback_FR")
    require(left["alias"] != right["alias"], "stereo sink aliases must differ")
    replacements = replace_endpoint(document, REFERENCE_LEFT_ALIAS, left)
    replacements += replace_endpoint(document, REFERENCE_RIGHT_ALIAS, right)
    require(
        replacements >= 2,
        f"expected at least two output endpoint replacements, found {replacements}",
    )
    return rendered_json_bytes(document), replacements, left, right


def verify_output_root(output_root: Path) -> Path:
    temporary_root = Path(tempfile.gettempdir()).resolve()
    resolved = output_root.resolve()
    require(
        is_descendant(resolved, temporary_root),
        f"output root must be a strict descendant of {temporary_root}",
    )
    require(
        not output_root.is_symlink(),
        "output root must not be a symbolic link",
    )
    if output_root.exists():
        require(output_root.is_dir(), "output root must be a directory")
        require(
            not any(output_root.iterdir()),
            "output root must be empty",
        )
    else:
        require(
            output_root.parent.resolve().is_dir(),
            f"output root parent does not exist: {output_root.parent}",
        )
    return resolved


def atomic_write(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Optional[Path] = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=str(path.parent),
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(str(temporary_path), str(path))
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def output_record(relative_path: str, content: bytes) -> Mapping[str, Any]:
    return {
        "path": relative_path,
        "bytes": len(content),
        "sha256": hashlib.sha256(content).hexdigest(),
    }


def materialize(arguments: argparse.Namespace) -> Mapping[str, Any]:
    rig_root = arguments.rig_root.resolve()
    repository_root = arguments.repository_root.resolve()
    definition = load_json(arguments.compiled_definition)
    require(isinstance(definition, dict), "compiled definition must be an object")
    binding = verify_compiled_definition(definition, rig_root)
    carla_source = verified_repository_source(
        binding,
        "carla-project-source",
        repository_root,
    )
    patchbay_source = verified_repository_source(
        binding,
        "patchbay-deployment-source",
        repository_root,
    )
    target_home = validate_posix_root(arguments.target_home, "target home")
    target_soundfont_root = validate_posix_root(
        arguments.target_soundfont_root,
        "target SoundFont root",
    )
    current_graph = load_json(arguments.current_graph)
    require(isinstance(current_graph, dict), "current graph must be an object")

    carla_content, carla_replacements = render_carla_project(
        carla_source,
        target_home,
        target_soundfont_root,
    )
    (
        patchbay_content,
        patchbay_replacements,
        left,
        right,
    ) = render_patchbay(
        patchbay_source,
        current_graph,
        arguments.default_node_name,
    )
    outputs = {
        "carla_project": output_record("carla/pedro.uproject", carla_content),
        "patchbay": output_record(
            "patchbay/pedro-live-rack-patchbay.json",
            patchbay_content,
        ),
    }
    manifest: Dict[str, Any] = {
        "schema": MATERIALIZATION_SCHEMA,
        "rig": definition["rig"],
        "rig_profile": definition["active_rig_profile"],
        "platform_binding": definition["platform_binding"],
        "definition_fingerprint": definition["definition_fingerprint"],
        "relocation": {
            "home": target_home,
            "soundfont_root": target_soundfont_root,
            "default_sink": {
                "node_name": arguments.default_node_name,
                "left": left,
                "right": right,
            },
            "carla_path_replacements": carla_replacements,
            "patchbay_endpoint_replacements": patchbay_replacements,
        },
        "outputs": outputs,
        "safety": {
            "activation": "authoring-only",
            "output_policy": "system-temporary-descendant",
            "materializes_runtime": False,
            "applies_graph_delta": False,
        },
    }
    manifest_content = rendered_json_bytes(manifest)

    if arguments.output_root is not None:
        output_root = verify_output_root(arguments.output_root)
        created_root = not output_root.exists()
        if created_root:
            output_root.mkdir()
        try:
            atomic_write(
                output_root / outputs["carla_project"]["path"],
                carla_content,
            )
            atomic_write(
                output_root / outputs["patchbay"]["path"],
                patchbay_content,
            )
            atomic_write(
                output_root / "materialization.json",
                manifest_content,
            )
        except Exception:
            if created_root:
                shutil.rmtree(output_root, ignore_errors=True)
            raise
    return manifest


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiled-definition", required=True, type=Path)
    parser.add_argument("--current-graph", required=True, type=Path)
    parser.add_argument("--default-node-name", required=True)
    parser.add_argument("--target-home", required=True)
    parser.add_argument("--target-soundfont-root", required=True)
    parser.add_argument("--rig-root", type=Path, default=DEFAULT_RIG_ROOT)
    parser.add_argument(
        "--repository-root",
        type=Path,
        default=REPOSITORY_ROOT,
    )
    output_mode = parser.add_mutually_exclusive_group(required=True)
    output_mode.add_argument(
        "--check-only",
        action="store_true",
        help="render and validate in memory without writing files",
    )
    output_mode.add_argument(
        "--output-root",
        type=Path,
        help="new or empty output directory below the system temporary root",
    )
    return parser.parse_args()


def main() -> int:
    try:
        arguments = parse_arguments()
        manifest = materialize(arguments)
        destination = (
            "memory"
            if arguments.output_root is None
            else str(arguments.output_root.resolve())
        )
        print(
            "Performance Rig materialization: PASS "
            f"(rig-profile={manifest['rig_profile']}, "
            f"platform-binding={manifest['platform_binding']['id']}, "
            f"carla-replacements="
            f"{manifest['relocation']['carla_path_replacements']}, "
            f"patchbay-replacements="
            f"{manifest['relocation']['patchbay_endpoint_replacements']}, "
            f"output={destination}, activation=none)"
        )
        return 0
    except MaterializationError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, TypeError, ET.ParseError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
