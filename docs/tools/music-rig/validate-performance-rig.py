#!/usr/bin/env python3
"""Validate portable Performance Rig v1 authored documents offline."""

from __future__ import annotations

import argparse
import json
import sys
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Sequence, Tuple

try:
    from jsonschema import Draft202012Validator
    from referencing import Registry, Resource
except ImportError as error:
    print(
        "Performance Rig schema validation requires dependencies from "
        "docs/tools/music-rig/requirements-schema.txt",
        file=sys.stderr,
    )
    raise SystemExit(2) from error


REQUIRED_JSONSCHEMA_VERSION = "4.23.0"
try:
    installed_jsonschema_version = version("jsonschema")
except PackageNotFoundError:
    installed_jsonschema_version = "missing"
if installed_jsonschema_version != REQUIRED_JSONSCHEMA_VERSION:
    print(
        "Performance Rig schema validation requires jsonschema=="
        f"{REQUIRED_JSONSCHEMA_VERSION}; found {installed_jsonschema_version}",
        file=sys.stderr,
    )
    raise SystemExit(2)


SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[3]
DEFAULT_RIG_ROOT = (
    REPOSITORY_ROOT / "src" / "performance-rigs" / "pedro-performance-rig"
)
DEFAULT_SCHEMA_DIR = DEFAULT_RIG_ROOT / "schemas"
DEFAULT_FIXTURE_DIR = SCRIPT_PATH.parent / "tests" / "fixtures" / "profile-schemas"

SCHEMA_FILE_BY_KIND = {
    "performance-rig": "rig.schema.json",
    "rig-profile": "rig-profile.schema.json",
    "device-profile": "device-profile.schema.json",
    "hardware-preset": "hardware-preset.schema.json",
    "switch-triggers": "switch-triggers.schema.json",
}
EXPECTED_SCHEMA_FILES = frozenset(
    ["common.schema.json"] + list(SCHEMA_FILE_BY_KIND.values())
)


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_schemas(schema_dir: Path) -> Dict[str, Mapping[str, Any]]:
    paths = sorted(schema_dir.glob("*.schema.json"))
    names = {path.name for path in paths}
    if names != EXPECTED_SCHEMA_FILES:
        missing = sorted(EXPECTED_SCHEMA_FILES - names)
        unexpected = sorted(names - EXPECTED_SCHEMA_FILES)
        raise ValueError(
            "schema set mismatch: "
            f"missing={missing or 'none'} unexpected={unexpected or 'none'}"
        )

    schemas: Dict[str, Mapping[str, Any]] = {}
    identifiers = set()
    for path in paths:
        schema = load_json(path)
        if not isinstance(schema, dict):
            raise ValueError(f"schema must be an object: {path}")
        identifier = schema.get("$id")
        if not isinstance(identifier, str) or not identifier:
            raise ValueError(f"schema has no $id: {path}")
        if identifier in identifiers:
            raise ValueError(f"duplicate schema $id: {identifier}")
        Draft202012Validator.check_schema(schema)
        identifiers.add(identifier)
        schemas[path.name] = schema
    return schemas


def build_registry(schemas: Mapping[str, Mapping[str, Any]]) -> Registry:
    resources = []
    for schema in schemas.values():
        resources.append((schema["$id"], Resource.from_contents(schema)))
    return Registry().with_resources(resources)


def error_path(parts: Iterable[Any]) -> str:
    rendered = "$"
    for part in parts:
        if isinstance(part, int):
            rendered += f"[{part}]"
        else:
            rendered += f".{part}"
    return rendered


def validate_document(
    document: Any,
    schemas: Mapping[str, Mapping[str, Any]],
    registry: Registry,
) -> Tuple[str, List[str]]:
    if not isinstance(document, dict):
        return "unknown", ["$: authored document must be an object"]
    kind = document.get("kind")
    if not isinstance(kind, str) or kind not in SCHEMA_FILE_BY_KIND:
        return str(kind or "unknown"), [f"$.kind: unsupported document kind {kind!r}"]

    schema = schemas[SCHEMA_FILE_BY_KIND[kind]]
    validator = Draft202012Validator(schema, registry=registry)
    errors = sorted(
        validator.iter_errors(document),
        key=lambda item: tuple(str(part) for part in item.absolute_path),
    )
    return kind, [
        f"{error_path(error.absolute_path)}: {error.message}" for error in errors
    ]


def run_self_test(schema_dir: Path, fixture_dir: Path) -> int:
    schemas = load_schemas(schema_dir)
    registry = build_registry(schemas)
    manifest_path = fixture_dir / "fixture-manifest.json"
    manifest = load_json(manifest_path)
    cases = manifest.get("cases") if isinstance(manifest, dict) else None
    if not isinstance(cases, list) or not cases:
        raise ValueError(f"fixture manifest has no cases: {manifest_path}")

    valid_count = 0
    invalid_count = 0
    covered_valid_kinds = set()
    covered_invalid_kinds = set()
    failures = []
    for case in cases:
        if not isinstance(case, dict):
            failures.append("fixture case must be an object")
            continue
        relative_path = case.get("path")
        expected_kind = case.get("kind")
        expected_valid = case.get("valid")
        expected_error = case.get("expected_error")
        if (
            not isinstance(relative_path, str)
            or expected_kind not in SCHEMA_FILE_BY_KIND
            or not isinstance(expected_valid, bool)
        ):
            failures.append(f"malformed fixture case: {case!r}")
            continue

        fixture_path = fixture_dir / relative_path
        document = load_json(fixture_path)
        actual_kind, errors = validate_document(document, schemas, registry)
        label = fixture_path.relative_to(fixture_dir).as_posix()
        if actual_kind != expected_kind:
            failures.append(
                f"{label}: kind {actual_kind!r}, expected {expected_kind!r}"
            )
            continue
        if expected_valid:
            valid_count += 1
            covered_valid_kinds.add(expected_kind)
            if errors:
                failures.append(f"{label}: expected valid; {'; '.join(errors)}")
        else:
            invalid_count += 1
            covered_invalid_kinds.add(expected_kind)
            if not errors:
                failures.append(f"{label}: expected validation failure")
            elif isinstance(expected_error, str) and not any(
                expected_error in error for error in errors
            ):
                failures.append(
                    f"{label}: expected error containing {expected_error!r}; "
                    f"got {'; '.join(errors)}"
                )

    expected_kinds = set(SCHEMA_FILE_BY_KIND)
    if covered_valid_kinds != expected_kinds:
        failures.append(
            "valid fixture kind coverage mismatch: "
            f"{sorted(covered_valid_kinds)}"
        )
    if covered_invalid_kinds != expected_kinds:
        failures.append(
            "invalid fixture kind coverage mismatch: "
            f"{sorted(covered_invalid_kinds)}"
        )
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(
        "Performance Rig schemas: PASS "
        f"(schemas={len(schemas)}, valid={valid_count}, invalid={invalid_count})"
    )
    return 0


def run_validation(schema_dir: Path, paths: Sequence[Path]) -> int:
    schemas = load_schemas(schema_dir)
    registry = build_registry(schemas)
    failed = False
    for path in paths:
        document = load_json(path)
        kind, errors = validate_document(document, schemas, registry)
        if errors:
            failed = True
            for error in errors:
                print(f"{path}: {error}", file=sys.stderr)
        else:
            print(f"{path}: OK ({kind})")
    return 1 if failed else 0


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--schema-dir",
        type=Path,
        default=DEFAULT_SCHEMA_DIR,
        help="directory containing the six Performance Rig v1 schemas",
    )
    parser.add_argument(
        "--fixture-dir",
        type=Path,
        default=DEFAULT_FIXTURE_DIR,
        help=argparse.SUPPRESS,
    )
    operation = parser.add_mutually_exclusive_group(required=True)
    operation.add_argument(
        "--self-test",
        action="store_true",
        help="validate all schemas plus positive and negative fixtures",
    )
    operation.add_argument(
        "--validate",
        type=Path,
        nargs="+",
        metavar="DOCUMENT",
        help="validate one or more authored JSON documents",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.self_test:
            return run_self_test(arguments.schema_dir, arguments.fixture_dir)
        return run_validation(arguments.schema_dir, arguments.validate)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
