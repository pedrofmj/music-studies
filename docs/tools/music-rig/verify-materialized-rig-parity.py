#!/usr/bin/env python3
"""Verify semantic parity between a temporary Rig bundle and protected sources."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple


sys.dont_write_bytecode = True


SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[3]
DEFAULT_REFERENCE_CARLA = (
    REPOSITORY_ROOT
    / "src"
    / "audio-software"
    / "carla"
    / "projects"
    / "pedro-live-rack"
    / "pedro.uproject"
)
DEFAULT_CARLA_METADATA = DEFAULT_REFERENCE_CARLA.with_name("project.json")
DEFAULT_REFERENCE_PATCHBAY = (
    REPOSITORY_ROOT
    / "src"
    / "midi-setup"
    / "pedro-live-rack-patchbay.json"
)
DEFAULT_AUTHORITY_SETUP = (
    REPOSITORY_ROOT / "docs" / "tools" / "airstar-live-setup" / "setup.json"
)
MATERIALIZATION_SCHEMA = "music-studies/materialized-performance-rig/v1"
PARITY_SCHEMA = "music-studies/materialized-performance-rig-parity/v1"
REFERENCE_HOME = "/home/ldap/pedro.ferreira"
REFERENCE_SOUNDFONT_ROOT = (
    REFERENCE_HOME + "/Flash/PED/MIDI/Pack de Timbres/Library"
)
REFERENCE_LEFT_ALIAS = "Speaker + Headphones:playback_FL"
REFERENCE_RIGHT_ALIAS = "Speaker + Headphones:playback_FR"
EXPECTED_OUTPUTS = {
    "carla_project": "carla/pedro.uproject",
    "patchbay": "patchbay/pedro-live-rack-patchbay.json",
}


class ParityError(Exception):
    """One or more deterministic parity failures."""

    def __init__(self, errors: Sequence[str]) -> None:
        self.errors = sorted(set(errors))
        super().__init__("; ".join(self.errors))


def canonical_json_bytes(document: Any) -> bytes:
    return json.dumps(
        document,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")


def fingerprint(document: Any) -> str:
    return "sha256:" + hashlib.sha256(canonical_json_bytes(document)).hexdigest()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def portable_text_sha256(path: Path) -> str:
    content = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(content).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ParityError([message])


def is_descendant(path: Path, parent: Path) -> bool:
    try:
        relative = path.relative_to(parent)
    except ValueError:
        return False
    return bool(relative.parts)


def resolve_bundle_file(root: Path, relative_path: str, label: str) -> Path:
    unresolved = root / relative_path
    require(not unresolved.is_symlink(), f"{label} must not be a symbolic link")
    candidate = unresolved.resolve()
    require(
        is_descendant(candidate, root),
        f"{label} path escapes the materialized root",
    )
    require(candidate.is_file(), f"{label} file does not exist: {candidate}")
    return candidate


def verify_bundle(root: Path) -> Tuple[Mapping[str, Any], Mapping[str, Path]]:
    require(not root.is_symlink(), "materialized root must not be a symbolic link")
    resolved_root = root.resolve()
    require(resolved_root.is_dir(), "materialized root must be a directory")
    temporary_root = Path(tempfile.gettempdir()).resolve()
    require(
        is_descendant(resolved_root, temporary_root),
        f"materialized root must be a strict descendant of {temporary_root}",
    )
    manifest_path = resolve_bundle_file(
        resolved_root,
        "materialization.json",
        "materialization manifest",
    )
    manifest = load_json(manifest_path)
    require(
        isinstance(manifest, dict),
        "materialization manifest must be an object",
    )
    require(
        manifest.get("schema") == MATERIALIZATION_SCHEMA,
        f"unsupported materialization schema: {manifest.get('schema')!r}",
    )
    require(
        manifest.get("rig") == "pedro-performance-rig"
        and manifest.get("rig_profile") == "full-live-rack",
        "materialization manifest is not the current full-live-rack",
    )
    platform_binding = manifest.get("platform_binding", {})
    require(
        isinstance(platform_binding, dict)
        and platform_binding.get("id") == "airstar-current"
        and platform_binding.get("platform") == "linux",
        "materialization manifest is not the airstar-current Linux binding",
    )
    require(
        manifest.get("safety")
        == {
            "activation": "authoring-only",
            "applies_graph_delta": False,
            "materializes_runtime": False,
            "output_policy": "system-temporary-descendant",
        },
        "materialization manifest changed its safety boundary",
    )
    outputs = manifest.get("outputs", {})
    require(isinstance(outputs, dict), "materialization outputs must be an object")
    require(
        set(outputs) == set(EXPECTED_OUTPUTS),
        f"unexpected materialization outputs: {sorted(outputs)}",
    )
    paths: Dict[str, Path] = {}
    errors = []
    for output_id, expected_path in sorted(EXPECTED_OUTPUTS.items()):
        record = outputs[output_id]
        if not isinstance(record, dict):
            errors.append(f"{output_id} record must be an object")
            continue
        if record.get("path") != expected_path:
            errors.append(
                f"{output_id} path: expected {expected_path!r}, "
                f"found {record.get('path')!r}"
            )
            continue
        path = resolve_bundle_file(resolved_root, expected_path, output_id)
        content = path.read_bytes()
        if record.get("bytes") != len(content):
            errors.append(
                f"{output_id} byte count: expected {record.get('bytes')!r}, "
                f"found {len(content)}"
            )
        digest = hashlib.sha256(content).hexdigest()
        if record.get("sha256") != digest:
            errors.append(f"{output_id} checksum mismatch")
        paths[output_id] = path
    if errors:
        raise ParityError(errors)
    return manifest, paths


def normalized_path(value: str, home: str, soundfont_root: str) -> str:
    replacements = (
        (soundfont_root, "<soundfont-root>"),
        (home, "<home>"),
    )
    for source, token in replacements:
        if value == source:
            return token
        if value.startswith(source + "/"):
            return token + value[len(source) :]
    return value


def canonical_element(
    element: ET.Element,
    home: str,
    soundfont_root: str,
) -> Mapping[str, Any]:
    text = (element.text or "").strip()
    if element.tag in ("Binary", "Filename") and text:
        text = normalized_path(text, home, soundfont_root)
    return {
        "tag": element.tag,
        "attributes": {
            key: element.attrib[key] for key in sorted(element.attrib)
        },
        "text": text,
        "children": [
            canonical_element(child, home, soundfont_root)
            for child in list(element)
        ],
    }


def carla_plugin_inventory(
    root: ET.Element,
    home: str,
    soundfont_root: str,
    label: str,
) -> Mapping[str, str]:
    plugins: Dict[str, str] = {}
    errors = []
    for plugin in root.findall("Plugin"):
        name = plugin.findtext("Info/Name")
        if not name:
            errors.append(f"{label} Carla plugin lacks Info/Name")
            continue
        if name in plugins:
            errors.append(f"{label} Carla plugin name is duplicated: {name!r}")
            continue
        plugins[name] = fingerprint(
            canonical_element(plugin, home, soundfont_root)
        )
    if errors:
        raise ParityError(errors)
    return plugins


def carla_asset_inventory(
    root: ET.Element,
    home: str,
    soundfont_root: str,
    label: str,
) -> Counter[Tuple[str, str, str]]:
    assets: Counter[Tuple[str, str, str]] = Counter()
    errors = []
    for plugin in root.findall("Plugin"):
        name = plugin.findtext("Info/Name")
        if not name:
            errors.append(f"{label} Carla plugin lacks Info/Name")
            continue
        for tag in ("Binary", "Filename"):
            for element in plugin.iter(tag):
                value = (element.text or "").strip()
                if value:
                    assets[
                        (
                            name,
                            tag,
                            normalized_path(value, home, soundfont_root),
                        )
                    ] += 1
    if errors:
        raise ParityError(errors)
    return assets


def carla_connection_inventory(
    root: ET.Element,
    label: str,
) -> Counter[Tuple[str, str]]:
    connections: Counter[Tuple[str, str]] = Counter()
    errors = []
    for connection in root.findall("ExternalPatchbay/Connection"):
        source = connection.findtext("Source")
        target = connection.findtext("Target")
        if not source or not target:
            errors.append(f"{label} Carla connection lacks Source or Target")
            continue
        connections[(source, target)] += 1
    if errors:
        raise ParityError(errors)
    return connections


def normalized_reference_endpoint(
    endpoint: Mapping[str, Any],
    relocation: Mapping[str, Any],
) -> Mapping[str, Any]:
    normalized = dict(endpoint)
    alias = endpoint.get("alias")
    default_sink = relocation.get("default_sink", {})
    if alias == REFERENCE_LEFT_ALIAS:
        replacement = default_sink.get("left", {})
    elif alias == REFERENCE_RIGHT_ALIAS:
        replacement = default_sink.get("right", {})
    else:
        replacement = {}
    if replacement:
        normalized["alias"] = replacement.get("alias")
        normalized["name_selector"] = replacement.get("name_selector")
    return normalized


def endpoint_semantics(endpoint: Mapping[str, Any]) -> Mapping[str, Any]:
    return {
        "direction": endpoint.get("direction"),
        "name": endpoint.get("name"),
        "name_selector": endpoint.get("name_selector"),
        "alias": endpoint.get("alias"),
        "format_dsp": endpoint.get("format_dsp"),
        "midi": endpoint.get("midi"),
    }


def patchbay_link_inventory(
    document: Mapping[str, Any],
    label: str,
    relocation: Optional[Mapping[str, Any]] = None,
) -> Counter[str]:
    require(isinstance(document, dict), f"{label} Patchbay must be an object")
    links = document.get("links")
    require(isinstance(links, list), f"{label} Patchbay links must be an array")
    inventory: Counter[str] = Counter()
    errors = []
    for position, link in enumerate(links):
        if not isinstance(link, dict):
            errors.append(f"{label} Patchbay link {position} must be an object")
            continue
        output = link.get("output")
        input_ = link.get("input")
        if not isinstance(output, dict) or not isinstance(input_, dict):
            errors.append(f"{label} Patchbay link {position} lacks endpoints")
            continue
        if relocation is not None:
            output = normalized_reference_endpoint(output, relocation)
            input_ = normalized_reference_endpoint(input_, relocation)
        semantic_link = {
            "kind": link.get("kind"),
            "output": endpoint_semantics(output),
            "input": endpoint_semantics(input_),
        }
        inventory[canonical_json_bytes(semantic_link).decode("utf-8")] += 1
    if errors:
        raise ParityError(errors)
    return inventory


def counter_records(counter: Counter[Any]) -> List[Mapping[str, Any]]:
    return [
        {"value": value, "multiplicity": count}
        for value, count in sorted(counter.items(), key=lambda item: str(item[0]))
    ]


def compare_plugins(
    reference: Mapping[str, str],
    materialized: Mapping[str, str],
) -> List[str]:
    reference_names = set(reference)
    materialized_names = set(materialized)
    missing = sorted(reference_names - materialized_names)
    unexpected = sorted(materialized_names - reference_names)
    changed = sorted(
        name
        for name in reference_names & materialized_names
        if reference[name] != materialized[name]
    )
    errors = []
    if missing:
        errors.append(f"Carla plugins missing: {missing}")
    if unexpected:
        errors.append(f"Carla plugins unexpected: {unexpected}")
    if changed:
        errors.append(f"Carla plugins changed: {changed}")
    return errors


def compare_counter(
    label: str,
    reference: Counter[Any],
    materialized: Counter[Any],
) -> List[str]:
    missing = reference - materialized
    unexpected = materialized - reference
    errors = []
    if missing:
        errors.append(f"{label} missing: {counter_records(missing)}")
    if unexpected:
        errors.append(f"{label} unexpected: {counter_records(unexpected)}")
    return errors


def require_count(label: str, actual: int, expected: int) -> List[str]:
    if actual == expected:
        return []
    return [f"{label}: expected {expected}, found {actual}"]


def verify_parity(arguments: argparse.Namespace) -> Mapping[str, Any]:
    manifest, output_paths = verify_bundle(arguments.materialized_root)
    metadata = load_json(arguments.carla_metadata)
    setup = load_json(arguments.authority_setup)
    expected_plugins = metadata["expected"]["plugins"]
    expected_connections = metadata["expected"]["carla_patchbay_connections"]
    expected_links = setup["patchbay"]["deployment_links"]
    errors = []
    errors.extend(
        require_count("protected expected Carla plugins", expected_plugins, 49)
    )
    errors.extend(
        require_count(
            "protected expected Carla connections",
            expected_connections,
            111,
        )
    )
    errors.extend(
        require_count("protected expected Patchbay links", expected_links, 115)
    )
    if metadata.get("sha256") != portable_text_sha256(arguments.reference_carla):
        errors.append("protected Carla metadata checksum mismatch")
    if (
        setup["patchbay"].get("deployment_sha256")
        != portable_text_sha256(arguments.reference_patchbay)
    ):
        errors.append("protected Patchbay setup checksum mismatch")
    if errors:
        raise ParityError(errors)

    reference_carla = ET.parse(arguments.reference_carla).getroot()
    materialized_carla = ET.parse(output_paths["carla_project"]).getroot()
    relocation = manifest["relocation"]
    reference_plugins = carla_plugin_inventory(
        reference_carla,
        REFERENCE_HOME,
        REFERENCE_SOUNDFONT_ROOT,
        "protected",
    )
    materialized_plugins = carla_plugin_inventory(
        materialized_carla,
        relocation["home"],
        relocation["soundfont_root"],
        "materialized",
    )
    reference_assets = carla_asset_inventory(
        reference_carla,
        REFERENCE_HOME,
        REFERENCE_SOUNDFONT_ROOT,
        "protected",
    )
    materialized_assets = carla_asset_inventory(
        materialized_carla,
        relocation["home"],
        relocation["soundfont_root"],
        "materialized",
    )
    reference_connections = carla_connection_inventory(
        reference_carla,
        "protected",
    )
    materialized_connections = carla_connection_inventory(
        materialized_carla,
        "materialized",
    )
    reference_patchbay = load_json(arguments.reference_patchbay)
    materialized_patchbay = load_json(output_paths["patchbay"])
    reference_links = patchbay_link_inventory(
        reference_patchbay,
        "protected",
        relocation,
    )
    materialized_links = patchbay_link_inventory(
        materialized_patchbay,
        "materialized",
    )

    errors = []
    errors.extend(
        require_count(
            "protected Carla plugin count",
            len(reference_plugins),
            expected_plugins,
        )
    )
    errors.extend(
        require_count(
            "materialized Carla plugin count",
            len(materialized_plugins),
            expected_plugins,
        )
    )
    errors.extend(
        require_count(
            "protected Carla connection count",
            sum(reference_connections.values()),
            expected_connections,
        )
    )
    errors.extend(
        require_count(
            "materialized Carla connection count",
            sum(materialized_connections.values()),
            expected_connections,
        )
    )
    errors.extend(
        require_count(
            "protected Patchbay link count",
            sum(reference_links.values()),
            expected_links,
        )
    )
    errors.extend(
        require_count(
            "materialized Patchbay link count",
            sum(materialized_links.values()),
            expected_links,
        )
    )
    errors.extend(compare_plugins(reference_plugins, materialized_plugins))
    errors.extend(
        compare_counter(
            "Carla plugin assets",
            reference_assets,
            materialized_assets,
        )
    )
    errors.extend(
        compare_counter(
            "Carla connections",
            reference_connections,
            materialized_connections,
        )
    )
    errors.extend(
        compare_counter("Patchbay links", reference_links, materialized_links)
    )
    if errors:
        raise ParityError(errors)

    plugin_records = [
        {"name": name, "fingerprint": reference_plugins[name]}
        for name in sorted(reference_plugins)
    ]
    return {
        "schema": PARITY_SCHEMA,
        "definition_fingerprint": manifest["definition_fingerprint"],
        "counts": {
            "carla_plugins": len(reference_plugins),
            "carla_plugin_assets": sum(reference_assets.values()),
            "carla_project_connections": sum(reference_connections.values()),
            "patchbay_links": sum(reference_links.values()),
        },
        "inventory_fingerprints": {
            "carla_plugins": fingerprint(plugin_records),
            "carla_plugin_assets": fingerprint(
                counter_records(reference_assets)
            ),
            "carla_project_connections": fingerprint(
                counter_records(reference_connections)
            ),
            "patchbay_links": fingerprint(counter_records(reference_links)),
        },
        "result": "semantic-parity",
    }


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--materialized-root", required=True, type=Path)
    parser.add_argument(
        "--reference-carla",
        type=Path,
        default=DEFAULT_REFERENCE_CARLA,
    )
    parser.add_argument(
        "--carla-metadata",
        type=Path,
        default=DEFAULT_CARLA_METADATA,
    )
    parser.add_argument(
        "--reference-patchbay",
        type=Path,
        default=DEFAULT_REFERENCE_PATCHBAY,
    )
    parser.add_argument(
        "--authority-setup",
        type=Path,
        default=DEFAULT_AUTHORITY_SETUP,
    )
    return parser.parse_args()


def main() -> int:
    try:
        result = verify_parity(parse_arguments())
        counts = result["counts"]
        print(
            "Materialized Rig semantic parity: PASS "
            f"(plugins={counts['carla_plugins']}, "
            f"plugin-assets={counts['carla_plugin_assets']}, "
            f"carla-connections={counts['carla_project_connections']}, "
            f"patchbay-links={counts['patchbay_links']}, "
            "activation=none)"
        )
        return 0
    except ParityError as error:
        for message in error.errors:
            print(f"ERROR: {message}", file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, TypeError, ET.ParseError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
