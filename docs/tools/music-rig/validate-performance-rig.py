#!/usr/bin/env python3
"""Validate portable Performance Rig v1 authored documents offline."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

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
SELECTOR_ORDER = {
    "model": 0,
    "semantic-alias": 1,
    "endpoint-purpose": 2,
    "local-discriminator": 3,
    "usb-id": 4,
}
IDENTIFIER_PATTERN = re.compile(r"^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$")
SEMANTIC_IDENTIFIER_PATTERN = re.compile(
    r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$"
)
USB_ID_PATTERN = re.compile(r"^[0-9a-f]{4}:[0-9a-f]{4}$")


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


def leaf_validation_errors(error: Any) -> Iterable[Any]:
    if error.context:
        for nested_error in error.context:
            yield from leaf_validation_errors(nested_error)
    else:
        yield error


def validate_rig_slot_semantics(document: Mapping[str, Any]) -> List[str]:
    """Validate selector relationships that JSON Schema cannot express."""
    errors = []
    slot_ids = set()
    semantic_alias_slots: Dict[str, str] = {}
    local_discriminator_slots: Dict[str, str] = {}
    usb_id_slots: Dict[str, List[Tuple[str, List[Mapping[str, Any]]]]] = (
        defaultdict(list)
    )

    for slot_index, slot in enumerate(document["device_slots"]):
        slot_path = f"$.device_slots[{slot_index}]"
        slot_id = slot["id"]
        if slot_id in slot_ids:
            errors.append(f"{slot_path}.id: duplicate device slot ID {slot_id!r}")
        slot_ids.add(slot_id)

        selectors = slot["selectors"]
        selector_keys = set()
        previous_rank = -1
        required_models = set()
        required_endpoint_selectors = set()

        for selector_index, selector in enumerate(selectors):
            selector_path = f"{slot_path}.selectors[{selector_index}]"
            selector_kind = selector["kind"]
            selector_value = selector["value"]
            selector_required = selector["required"]
            selector_key = (selector_kind, selector_value)

            if selector_key in selector_keys:
                errors.append(
                    f"{selector_path}: duplicate selector {selector_kind!r} "
                    f"with value {selector_value!r}"
                )
            selector_keys.add(selector_key)

            selector_rank = SELECTOR_ORDER[selector_kind]
            if selector_rank < previous_rank:
                errors.append(
                    f"{selector_path}: selector order must be model, "
                    "semantic-alias, endpoint-purpose, local-discriminator, "
                    "then usb-id"
                )
            previous_rank = selector_rank

            if selector_kind == "model" and selector_required:
                required_models.add(selector_value)
            elif selector_kind == "endpoint-purpose":
                if not SEMANTIC_IDENTIFIER_PATTERN.fullmatch(selector_value):
                    errors.append(
                        f"{selector_path}.value: endpoint purpose must be a "
                        "semantic identifier"
                    )
                if selector_required:
                    required_endpoint_selectors.add(selector_value)
            elif selector_kind == "semantic-alias":
                if not SEMANTIC_IDENTIFIER_PATTERN.fullmatch(selector_value):
                    errors.append(
                        f"{selector_path}.value: semantic alias must be a "
                        "semantic identifier"
                    )
                previous_slot = semantic_alias_slots.get(selector_value)
                if previous_slot is not None and previous_slot != slot_id:
                    errors.append(
                        f"{selector_path}.value: semantic alias "
                        f"{selector_value!r} is already used by slot "
                        f"{previous_slot!r}"
                    )
                semantic_alias_slots[selector_value] = slot_id
            elif selector_kind == "local-discriminator":
                if not IDENTIFIER_PATTERN.fullmatch(selector_value):
                    errors.append(
                        f"{selector_path}.value: local discriminator must be "
                        "an identifier"
                    )
                if selector_required:
                    errors.append(
                        f"{selector_path}.required: local discriminator must "
                        "be optional"
                    )
                previous_slot = local_discriminator_slots.get(selector_value)
                if previous_slot is not None and previous_slot != slot_id:
                    errors.append(
                        f"{selector_path}.value: local discriminator "
                        f"{selector_value!r} is already used by slot "
                        f"{previous_slot!r}"
                    )
                local_discriminator_slots[selector_value] = slot_id
            elif selector_kind == "usb-id":
                if not USB_ID_PATTERN.fullmatch(selector_value):
                    errors.append(
                        f"{selector_path}.value: USB ID must use lower-case "
                        "hhhh:hhhh notation"
                    )
                if selector_required:
                    errors.append(
                        f"{selector_path}.required: USB ID must be optional"
                    )
                usb_id_slots[selector_value].append((slot_id, selectors))

        if not required_models.intersection(slot["compatible_models"]):
            errors.append(
                f"{slot_path}.selectors: a required model selector must match "
                "compatible_models"
            )

        for endpoint in slot["required_endpoints"]:
            if endpoint not in required_endpoint_selectors:
                errors.append(
                    f"{slot_path}.selectors: required endpoint {endpoint!r} "
                    "must have a matching required endpoint-purpose selector"
                )

    for usb_id, slot_entries in usb_id_slots.items():
        affected_slot_ids = {slot_id for slot_id, _ in slot_entries}
        if len(affected_slot_ids) < 2:
            continue
        for slot_id, selectors in slot_entries:
            has_required_alias = any(
                selector["kind"] == "semantic-alias" and selector["required"]
                for selector in selectors
            )
            has_optional_local_discriminator = any(
                selector["kind"] == "local-discriminator"
                and not selector["required"]
                for selector in selectors
            )
            if not has_required_alias or not has_optional_local_discriminator:
                errors.append(
                    f"$.device_slots: shared USB ID {usb_id!r} requires slot "
                    f"{slot_id!r} to have a required semantic alias and an "
                    "optional local discriminator"
                )

    return errors


def is_wildcard_note_stream(message: Mapping[str, Any]) -> bool:
    return message.get("channel") == "any" and message.get("number") == "any"


def validate_hardware_preset_semantics(
    document: Mapping[str, Any],
) -> List[str]:
    """Validate control identities and evidence constraints within a preset."""
    errors = []
    control_ids = set()
    message_controls: Dict[Tuple[Any, Any, Any], str] = {}
    verification_status = document["verification"]["status"]

    for control_index, control in enumerate(document["controls"]):
        control_path = f"$.controls[{control_index}]"
        control_id = control["id"]
        if control_id in control_ids:
            errors.append(
                f"{control_path}.id: duplicate hardware control ID {control_id!r}"
            )
        control_ids.add(control_id)

        message = control["message"]
        message_key = (
            message["type"],
            message["channel"],
            message["number"],
        )
        previous_control = message_controls.get(message_key)
        if previous_control is not None and previous_control != control_id:
            errors.append(
                f"{control_path}.message: MIDI message collides with control "
                f"{previous_control!r}"
            )
        message_controls[message_key] = control_id

        wildcard = is_wildcard_note_stream(message)
        if verification_status == "verified" and wildcard:
            errors.append(
                f"{control_path}.message: verified preset cannot contain "
                "wildcard note evidence"
            )
        if wildcard and control["behavior"] != "momentary":
            errors.append(
                f"{control_path}.behavior: wildcard note stream must be "
                "momentary"
            )
        if control["behavior"] == "relative" and message["type"] != "cc":
            errors.append(
                f"{control_path}.message: relative control must emit CC"
            )

        has_off_value = "off_value" in control
        has_on_value = "on_value" in control
        if has_off_value != has_on_value:
            errors.append(
                f"{control_path}: off_value and on_value must be specified "
                "together"
            )
        elif has_off_value and control["off_value"] == control["on_value"]:
            errors.append(
                f"{control_path}: off_value and on_value must differ"
            )

    return errors


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
        (
            leaf_error
            for error in validator.iter_errors(document)
            for leaf_error in leaf_validation_errors(error)
        ),
        key=lambda item: tuple(str(part) for part in item.absolute_path),
    )
    formatted_errors = [
        f"{error_path(error.absolute_path)}: {error.message}" for error in errors
    ]
    if not formatted_errors and kind == "performance-rig":
        formatted_errors.extend(validate_rig_slot_semantics(document))
    elif not formatted_errors and kind == "hardware-preset":
        formatted_errors.extend(validate_hardware_preset_semantics(document))
    return kind, formatted_errors


def validate_hardware_catalogue_documents(
    rig: Mapping[str, Any],
    preset_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
) -> Tuple[List[str], int, int]:
    """Cross-check the Rig's Hardware Preset catalogue and device models."""
    errors = []
    preset_by_id: Dict[str, Tuple[Path, Mapping[str, Any]]] = {}
    verified_count = 0
    partial_count = 0

    for path, preset in preset_entries:
        preset_id = preset["id"]
        previous = preset_by_id.get(preset_id)
        if previous is not None:
            errors.append(
                f"{path}: duplicate hardware preset ID {preset_id!r}; "
                f"already defined by {previous[0]}"
            )
        else:
            preset_by_id[preset_id] = (path, preset)
        if path.stem != preset_id:
            errors.append(
                f"{path}: file name must match hardware preset ID {preset_id!r}"
            )

        verification_status = preset["verification"]["status"]
        if verification_status == "verified":
            verified_count += 1
        else:
            partial_count += 1

        matching_slots = [
            slot["id"]
            for slot in rig["device_slots"]
            if set(preset["device_models"]).intersection(slot["compatible_models"])
        ]
        if not matching_slots:
            errors.append(
                f"{path}: device_models do not match any Rig device slot"
            )

    expected_ids = set(rig["hardware_presets"])
    actual_ids = set(preset_by_id)
    for preset_id in sorted(expected_ids - actual_ids):
        errors.append(
            f"rig.json: unresolved hardware preset {preset_id!r}"
        )
    for preset_id in sorted(actual_ids - expected_ids):
        errors.append(
            f"{preset_by_id[preset_id][0]}: hardware preset {preset_id!r} "
            "is not declared by rig.json"
        )

    for slot in rig["device_slots"]:
        if not slot.get("required", True):
            continue
        has_compatible_preset = any(
            set(preset["device_models"]).intersection(slot["compatible_models"])
            for _, preset in preset_entries
        )
        if not has_compatible_preset:
            errors.append(
                f"rig.json: required slot {slot['id']!r} has no compatible "
                "hardware preset"
            )

    return errors, verified_count, partial_count


def validate_hardware_catalogue(
    rig_root: Path,
    schemas: Mapping[str, Mapping[str, Any]],
    registry: Registry,
) -> Tuple[List[str], int, int]:
    rig_path = rig_root / "rig.json"
    rig = load_json(rig_path)
    rig_kind, rig_errors = validate_document(rig, schemas, registry)
    errors = [f"{rig_path}: {error}" for error in rig_errors]
    if rig_kind != "performance-rig":
        errors.append(f"{rig_path}: expected a performance-rig document")

    preset_entries = []
    for preset_path in sorted((rig_root / "hardware-presets").glob("*.json")):
        preset = load_json(preset_path)
        preset_kind, preset_errors = validate_document(preset, schemas, registry)
        errors.extend(f"{preset_path}: {error}" for error in preset_errors)
        if preset_kind != "hardware-preset":
            errors.append(f"{preset_path}: expected a hardware-preset document")
        if not preset_errors and preset_kind == "hardware-preset":
            preset_entries.append((preset_path, preset))

    if errors:
        return errors, 0, 0
    semantic_errors, verified_count, partial_count = (
        validate_hardware_catalogue_documents(rig, preset_entries)
    )
    return semantic_errors, verified_count, partial_count


def control_signature(control: Mapping[str, Any]) -> Mapping[str, Any]:
    message = control["message"]
    signature = {
        "behavior": control["behavior"],
        "type": message["type"],
        "channel": message["channel"],
        "number": message["number"],
    }
    for field in ("off_value", "on_value", "relative_encoding"):
        if field in control:
            signature[field] = control[field]
    return signature


def expected_current_control_signatures(
    setup: Mapping[str, Any],
) -> Mapping[str, Mapping[str, Mapping[str, Any]]]:
    controller_profile = setup["controller_profile"]
    arturia = controller_profile["arturia"]
    smk25 = controller_profile["smk25"]
    mixer = controller_profile["smc_mixer"]

    expected: Dict[str, Dict[str, Mapping[str, Any]]] = {
        "arturia-current-rack": {},
        "smk25-current-pad-layers": {},
        "smc-mixer-current-cc": {},
    }
    for index, cc_number in enumerate(arturia["fader_ccs"], start=1):
        expected["arturia-current-rack"][f"fader-{index}"] = {
            "behavior": "absolute",
            "type": "cc",
            "channel": arturia["channel"],
            "number": cc_number,
        }
    for index, cc_number in enumerate(arturia["instrument_knob_ccs"], start=1):
        expected["arturia-current-rack"][f"knob-{index}"] = {
            "behavior": "absolute",
            "type": "cc",
            "channel": arturia["channel"],
            "number": cc_number,
        }
    expected["arturia-current-rack"]["central-encoder"] = {
        "behavior": "relative",
        "type": "cc",
        "channel": arturia["channel"],
        "number": arturia["central_encoder"]["input_cc"],
        "relative_encoding": "binary-offset",
    }
    expected["arturia-current-rack"]["central-click"] = {
        "behavior": "momentary",
        "type": "cc",
        "channel": arturia["channel"],
        "number": arturia["central_click"]["cc"],
        "off_value": 0,
        "on_value": 127,
    }

    for index, cc_number in enumerate(smk25["knobs"]["ccs"], start=1):
        expected["smk25-current-pad-layers"][f"knob-{index}"] = {
            "behavior": "absolute",
            "type": "cc",
            "channel": smk25["knobs"]["channel"],
            "number": cc_number,
        }
    for index, (cc_number, channel) in enumerate(
        zip(smk25["side_a_pads"]["ccs"], smk25["side_a_pads"]["channels"]),
        start=1,
    ):
        expected["smk25-current-pad-layers"][f"side-a-pad-{index}"] = {
            "behavior": "toggle",
            "type": "cc",
            "channel": channel,
            "number": cc_number,
            "off_value": smk25["side_a_pads"]["off_value"],
            "on_value": smk25["side_a_pads"]["on_value"],
        }
    for control_id, note_field in (
        ("transport-stop", "stop_note"),
        ("transport-play", "play_note"),
    ):
        expected["smk25-current-pad-layers"][control_id] = {
            "behavior": "momentary",
            "type": "note",
            "channel": smk25["transport"]["channel"],
            "number": smk25["transport"][note_field],
        }

    for index, cc_number in enumerate(mixer["fader_ccs"], start=1):
        expected["smc-mixer-current-cc"][f"fader-{index}"] = {
            "behavior": "absolute",
            "type": "cc",
            "channel": mixer["channel"],
            "number": cc_number,
        }
    return expected


def validate_current_hardware_extraction(
    rig_root: Path,
    setup_path: Path,
) -> List[str]:
    """Compare fully verified presets with the protected structured evidence."""
    setup = load_json(setup_path)
    expected_by_preset = expected_current_control_signatures(setup)
    errors = []

    for preset_id, expected_controls in expected_by_preset.items():
        preset_path = rig_root / "hardware-presets" / f"{preset_id}.json"
        preset = load_json(preset_path)
        if preset["verification"]["status"] != "verified":
            errors.append(
                f"{preset_path}: protected extraction must remain verified"
            )
        actual_controls = {
            control["id"]: control_signature(control)
            for control in preset["controls"]
        }
        if set(actual_controls) != set(expected_controls):
            errors.append(
                f"{preset_path}: control ID set differs from protected setup; "
                f"expected={sorted(expected_controls)} "
                f"actual={sorted(actual_controls)}"
            )
            continue
        for control_id, expected_signature in expected_controls.items():
            actual_signature = actual_controls[control_id]
            if actual_signature != expected_signature:
                errors.append(
                    f"{preset_path}: control {control_id!r} differs from "
                    f"protected setup; expected={expected_signature} "
                    f"actual={actual_signature}"
                )
    return errors


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

    fixture_rig = load_json(fixture_dir / "valid" / "rig.json")
    fixture_preset_path = Path("hardware-presets/fixture-preset.json")
    fixture_preset = load_json(fixture_dir / "valid" / "hardware-preset.json")
    catalogue_errors, _, _ = validate_hardware_catalogue_documents(
        fixture_rig,
        [(fixture_preset_path, fixture_preset)],
    )
    if catalogue_errors:
        failures.append(
            "valid hardware catalogue failed: "
            f"{'; '.join(catalogue_errors)}"
        )

    unresolved_rig = dict(fixture_rig)
    unresolved_rig["hardware_presets"] = ["missing-preset"]
    catalogue_errors, _, _ = validate_hardware_catalogue_documents(
        unresolved_rig,
        [(fixture_preset_path, fixture_preset)],
    )
    if not any("unresolved hardware preset" in error for error in catalogue_errors):
        failures.append(
            "invalid hardware catalogue did not reject an unresolved reference"
        )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(
        "Performance Rig schemas: PASS "
        f"(schemas={len(schemas)}, valid={valid_count}, invalid={invalid_count}, "
        "catalogues=2)"
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


def run_root_validation(
    schema_dir: Path,
    rig_root: Path,
    authority_setup: Optional[Path],
) -> int:
    schemas = load_schemas(schema_dir)
    registry = build_registry(schemas)
    errors, verified_count, partial_count = validate_hardware_catalogue(
        rig_root,
        schemas,
        registry,
    )
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1
    if authority_setup is not None:
        extraction_errors = validate_current_hardware_extraction(
            rig_root,
            authority_setup,
        )
        if extraction_errors:
            for error in extraction_errors:
                print(error, file=sys.stderr)
            return 1
    print(
        f"{rig_root}: OK (hardware-presets="
        f"{verified_count + partial_count}, verified={verified_count}, "
        f"partial={partial_count})"
    )
    return 0


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
    parser.add_argument(
        "--authority-setup",
        type=Path,
        help="protected setup.json used for current-preset extraction parity",
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
    operation.add_argument(
        "--validate-root",
        type=Path,
        metavar="RIG_ROOT",
        help="validate the authored Rig and its Hardware Preset catalogue",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.authority_setup is not None and not arguments.validate_root:
            raise ValueError("--authority-setup requires --validate-root")
        if arguments.self_test:
            return run_self_test(arguments.schema_dir, arguments.fixture_dir)
        if arguments.validate_root:
            return run_root_validation(
                arguments.schema_dir,
                arguments.validate_root,
                arguments.authority_setup,
            )
        return run_validation(arguments.schema_dir, arguments.validate)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
