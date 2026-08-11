#!/usr/bin/env python3
"""Compile validated Performance Rig sources into a deterministic envelope."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path
from types import ModuleType
from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple


sys.dont_write_bytecode = True


SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[3]
DEFAULT_RIG_ROOT = (
    REPOSITORY_ROOT / "src" / "performance-rigs" / "pedro-performance-rig"
)
VALIDATOR_PATH = SCRIPT_PATH.with_name("validate-performance-rig.py")
COMPILED_SCHEMA = "music-studies/compiled-performance-rig/v1"
COMPILER_ID = "music-rig-authoring-compiler"
COMPILER_VERSION = "0.1.0"


class CompilationError(Exception):
    """A deterministic list of source or invocation failures."""

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


def rendered_json_bytes(document: Any) -> bytes:
    return (
        json.dumps(
            document,
            ensure_ascii=True,
            indent=2,
            sort_keys=True,
        )
        + "\n"
    ).encode("utf-8")


def fingerprint(document: Any) -> str:
    digest = hashlib.sha256(canonical_json_bytes(document)).hexdigest()
    return f"sha256:{digest}"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_validator() -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "music_rig_profile_validator",
        VALIDATOR_PATH,
    )
    if spec is None or spec.loader is None:
        raise CompilationError([f"cannot load validator: {VALIDATOR_PATH}"])
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def normalized_error(error: str, rig_root: Path) -> str:
    normalized = error.replace(str(rig_root), "<rig-root>")
    return normalized.replace("\\", "/")


def validate_catalogue(rig_root: Path, validator: ModuleType) -> None:
    schema_dir = rig_root / "schemas"
    schemas = validator.load_schemas(schema_dir)
    registry = validator.build_registry(schemas)
    result = validator.validate_authored_catalogue(
        rig_root,
        schemas,
        registry,
    )
    errors = result[0]
    if errors:
        raise CompilationError(
            [normalized_error(error, rig_root) for error in errors]
        )


def relative_posix_path(path: Path, rig_root: Path) -> str:
    return path.relative_to(rig_root).as_posix()


def document_manifest(
    paths: Sequence[Path],
    rig_root: Path,
) -> List[Mapping[str, str]]:
    entries = []
    for path in sorted(paths, key=lambda item: relative_posix_path(item, rig_root)):
        document = load_json(path)
        entries.append(
            {
                "path": relative_posix_path(path, rig_root),
                "fingerprint": fingerprint(document),
            }
        )
    return entries


def portable_source_paths(rig_root: Path) -> List[Path]:
    paths = [rig_root / "rig.json", rig_root / "switch-triggers.json"]
    paths.extend(sorted((rig_root / "hardware-presets").glob("*.json")))
    paths.extend(sorted((rig_root / "device-profiles").glob("*/*.json")))
    paths.extend(sorted((rig_root / "rig-profiles").glob("*.json")))
    return paths


def resolve_binding_path(rig_root: Path, binding_id: str) -> Path:
    matches = sorted(
        path
        for path in (rig_root / "platform-bindings").glob("*/*.json")
        if path.stem == binding_id
    )
    if not matches:
        raise CompilationError(
            [f"unresolved Platform Binding {binding_id!r}"]
        )
    if len(matches) > 1:
        rendered = [relative_posix_path(path, rig_root) for path in matches]
        raise CompilationError(
            [
                f"ambiguous Platform Binding {binding_id!r}: "
                f"{rendered}"
            ]
        )
    return matches[0]


def load_profiles(
    rig_root: Path,
) -> Tuple[
    Mapping[str, Mapping[str, Any]],
    Mapping[Tuple[str, str], Mapping[str, Any]],
]:
    rig_profiles = {}
    for path in sorted((rig_root / "rig-profiles").glob("*.json")):
        profile = load_json(path)
        rig_profiles[profile["id"]] = profile

    device_profiles = {}
    for path in sorted((rig_root / "device-profiles").glob("*/*.json")):
        profile = load_json(path)
        device_profiles[(profile["slot"], profile["id"])] = profile
    return rig_profiles, device_profiles


def compile_definition(
    rig_root: Path,
    binding_id: str,
    rig_profile_id: Optional[str],
) -> Mapping[str, Any]:
    rig_root = rig_root.resolve()
    validator = load_validator()
    validate_catalogue(rig_root, validator)

    rig = load_json(rig_root / "rig.json")
    binding_path = resolve_binding_path(rig_root, binding_id)
    binding = load_json(binding_path)
    if binding_id not in rig["platform_bindings"]:
        raise CompilationError(
            [f"Platform Binding {binding_id!r} is not declared by rig.json"]
        )

    selected_rig_profile_id = (
        rig_profile_id or rig.get("default_rig_profile")
    )
    if selected_rig_profile_id is None:
        raise CompilationError(["rig.json has no default Rig Profile"])

    rig_profiles, device_profiles = load_profiles(rig_root)
    rig_profile = rig_profiles.get(selected_rig_profile_id)
    if rig_profile is None:
        raise CompilationError(
            [f"unresolved Rig Profile {selected_rig_profile_id!r}"]
        )
    if selected_rig_profile_id not in binding["rig_profiles"]:
        raise CompilationError(
            [
                f"Platform Binding {binding_id!r} does not provide Rig "
                f"Profile {selected_rig_profile_id!r}"
            ]
        )

    compiled_profiles = []
    resource_requirements: Dict[str, set] = {
        "assets": set(),
        "effects": set(),
        "engines": set(),
        "helpers": set(),
        "routes": set(),
    }
    for slot_id, profile_id in sorted(
        rig_profile["device_profiles"].items()
    ):
        profile = device_profiles[(slot_id, profile_id)]
        compiled_profiles.append(
            {
                "slot": slot_id,
                "profile": profile_id,
                "hardware_preset": profile["hardware_preset"],
                "readiness": profile["readiness"],
            }
        )
        for category, resources in profile["dependencies"].items():
            resource_requirements[category].update(resources)
    for category, resources in rig_profile.get("shared_resources", {}).items():
        resource_requirements[category].update(resources)

    schema_paths = sorted((rig_root / "schemas").glob("*.schema.json"))
    portable_paths = portable_source_paths(rig_root)
    schema_manifest = document_manifest(schema_paths, rig_root)
    portable_manifest = document_manifest(portable_paths, rig_root)
    binding_fingerprint = fingerprint(binding)

    definition: Dict[str, Any] = {
        "schema": COMPILED_SCHEMA,
        "compiler": {
            "id": COMPILER_ID,
            "version": COMPILER_VERSION,
        },
        "generation": 1,
        "rig": rig["id"],
        "active_rig_profile": selected_rig_profile_id,
        "readiness": rig_profile["readiness"],
        "device_profiles": compiled_profiles,
        "platform_binding": {
            "id": binding["id"],
            "platform": binding["platform"],
            "evidence_status": binding["evidence_status"],
        },
        "requirements": {
            "capabilities": sorted(rig_profile["required_capabilities"]),
            "pinned_capabilities": sorted(
                rig_profile["preparation"]["pinned_capabilities"]
            ),
            "resources": {
                category: sorted(resources)
                for category, resources in sorted(
                    resource_requirements.items()
                )
            },
        },
        "fingerprints": {
            "algorithm": "sha256",
            "schema_set": fingerprint(schema_manifest),
            "portable_source": fingerprint(portable_manifest),
            "platform_binding": binding_fingerprint,
        },
        "source_manifest": {
            "schemas": schema_manifest,
            "portable": portable_manifest,
            "platform_binding": {
                "path": relative_posix_path(binding_path, rig_root),
                "fingerprint": binding_fingerprint,
            },
        },
        "safety": {
            "activation": "authoring-only",
            "materializes_runtime": False,
        },
    }
    definition["definition_fingerprint"] = fingerprint(definition)
    return definition


def verify_output_target(output: Path, rig_root: Path) -> None:
    resolved_output = output.resolve()
    source_paths = portable_source_paths(rig_root)
    source_paths.extend((rig_root / "schemas").glob("*.schema.json"))
    source_paths.extend((rig_root / "platform-bindings").glob("*/*.json"))
    if resolved_output in {path.resolve() for path in source_paths}:
        raise CompilationError(
            [f"output path would overwrite source document: {output}"]
        )
    if not resolved_output.parent.is_dir():
        raise CompilationError(
            [f"output directory does not exist: {output.parent}"]
        )


def write_output(output: Path, content: bytes) -> None:
    resolved_output = output.resolve()
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=str(resolved_output.parent),
            prefix=f".{output.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(str(temporary_path), str(resolved_output))
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rig-root",
        type=Path,
        default=DEFAULT_RIG_ROOT,
        help="validated authored Performance Rig root",
    )
    parser.add_argument(
        "--platform-binding",
        required=True,
        help="explicit Platform Binding ID",
    )
    parser.add_argument(
        "--rig-profile",
        help="Rig Profile ID; defaults to rig.json default_rig_profile",
    )
    output_mode = parser.add_mutually_exclusive_group(required=True)
    output_mode.add_argument(
        "--check-only",
        action="store_true",
        help="validate and compile in memory without writing output",
    )
    output_mode.add_argument(
        "--output",
        type=Path,
        help="explicit compiled JSON output path",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        rig_root = arguments.rig_root.resolve()
        definition = compile_definition(
            rig_root,
            arguments.platform_binding,
            arguments.rig_profile,
        )
        content = rendered_json_bytes(definition)
        if arguments.output is not None:
            verify_output_target(arguments.output, rig_root)
            write_output(arguments.output, content)
        print(
            "Performance Rig compile: PASS "
            f"(rig={definition['rig']}, "
            f"rig-profile={definition['active_rig_profile']}, "
            f"platform-binding={definition['platform_binding']['id']}, "
            f"portable-source={definition['fingerprints']['portable_source']}, "
            f"binding={definition['fingerprints']['platform_binding']}, "
            f"definition={definition['definition_fingerprint']})"
        )
        return 0
    except CompilationError as error:
        for message in error.errors:
            print(f"ERROR: {message}", file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
