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

    for internal_index, internal_control in enumerate(
        document.get("internal_controls", [])
    ):
        internal_path = f"$.internal_controls[{internal_index}]"
        internal_id = internal_control["id"]
        if internal_id in control_ids:
            errors.append(
                f"{internal_path}.id: duplicate hardware control ID "
                f"{internal_id!r}"
            )
        control_ids.add(internal_id)

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


def validate_device_profile_semantics(
    document: Mapping[str, Any],
) -> List[str]:
    """Validate mappings and ownership relationships within one profile."""
    errors = []
    mapping_ids = set()
    ownership_keys = set()

    for claim_index, claim in enumerate(document["ownership"]):
        claim_path = f"$.ownership[{claim_index}]"
        claim_key = (claim["kind"], claim["target"])
        if claim_key in ownership_keys:
            errors.append(
                f"{claim_path}: duplicate ownership claim for "
                f"{claim['kind']!r} target {claim['target']!r}"
            )
        ownership_keys.add(claim_key)

    owned_targets = {claim["target"] for claim in document["ownership"]}
    for mapping_index, mapping in enumerate(document["mappings"]):
        mapping_path = f"$.mappings[{mapping_index}]"
        mapping_id = mapping["id"]
        if mapping_id in mapping_ids:
            errors.append(
                f"{mapping_path}.id: duplicate mapping ID {mapping_id!r}"
            )
        mapping_ids.add(mapping_id)
        if mapping["target"] not in owned_targets:
            errors.append(
                f"{mapping_path}.target: target {mapping['target']!r} has no "
                "ownership claim"
            )

    for state_key in document.get("default_state", {}):
        if ("state-key", state_key) not in ownership_keys:
            errors.append(
                f"$.default_state.{state_key}: state key has no state-key "
                "ownership claim"
            )

    return errors


def validate_rig_profile_semantics(
    document: Mapping[str, Any],
) -> List[str]:
    """Validate ownership identities within one global profile."""
    errors = []
    ownership_keys = set()
    for claim_index, claim in enumerate(document["ownership"]):
        claim_path = f"$.ownership[{claim_index}]"
        claim_key = (claim["kind"], claim["target"])
        if claim_key in ownership_keys:
            errors.append(
                f"{claim_path}: duplicate ownership claim for "
                f"{claim['kind']!r} target {claim['target']!r}"
            )
        ownership_keys.add(claim_key)
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
    elif not formatted_errors and kind == "rig-profile":
        formatted_errors.extend(validate_rig_profile_semantics(document))
    elif not formatted_errors and kind == "device-profile":
        formatted_errors.extend(validate_device_profile_semantics(document))
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


def validate_device_profile_catalogue_documents(
    rig: Mapping[str, Any],
    preset_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
    profile_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
) -> List[str]:
    """Cross-check profile slots, presets, controls, and current ownership."""
    errors = []
    slots_by_id = {slot["id"]: slot for slot in rig["device_slots"]}
    presets_by_id = {preset["id"]: preset for _, preset in preset_entries}
    profiles_by_key: Dict[
        Tuple[str, str], Tuple[Path, Mapping[str, Any]]
    ] = {}

    for path, profile in profile_entries:
        profile_key = (profile["slot"], profile["id"])
        previous = profiles_by_key.get(profile_key)
        if previous is not None:
            errors.append(
                f"{path}: duplicate Device Profile {profile_key!r}; "
                f"already defined by {previous[0]}"
            )
        else:
            profiles_by_key[profile_key] = (path, profile)
        if path.stem != profile["id"] or path.parent.name != profile["slot"]:
            errors.append(
                f"{path}: file layout must be "
                f"device-profiles/{profile['slot']}/{profile['id']}.json"
            )

        slot = slots_by_id.get(profile["slot"])
        if slot is None:
            errors.append(
                f"{path}: unresolved device slot {profile['slot']!r}"
            )
            continue
        if not set(profile["compatible_models"]).issubset(
            slot["compatible_models"]
        ):
            errors.append(
                f"{path}: compatible_models are not covered by slot "
                f"{profile['slot']!r}"
            )
        if not set(profile["required_endpoints"]).issubset(
            slot["required_endpoints"]
        ):
            errors.append(
                f"{path}: required_endpoints are not provided by slot "
                f"{profile['slot']!r}"
            )

        preset = presets_by_id.get(profile["hardware_preset"])
        if preset is None:
            errors.append(
                f"{path}: unresolved hardware preset "
                f"{profile['hardware_preset']!r}"
            )
            continue
        if not set(profile["compatible_models"]).intersection(
            preset["device_models"]
        ):
            errors.append(
                f"{path}: hardware preset {profile['hardware_preset']!r} "
                "does not support a compatible profile model"
            )
        preset_controls = {control["id"] for control in preset["controls"]}
        for mapping_index, mapping in enumerate(profile["mappings"]):
            if mapping["source_control"] not in preset_controls:
                errors.append(
                    f"{path}: $.mappings[{mapping_index}].source_control: "
                    f"control {mapping['source_control']!r} is not defined by "
                    f"hardware preset {profile['hardware_preset']!r}"
                )

    expected_profile_keys = {
        (slot["id"], profile_id)
        for slot in rig["device_slots"]
        for profile_id in slot["available_device_profiles"]
    }
    actual_profile_keys = set(profiles_by_key)
    for profile_key in sorted(expected_profile_keys - actual_profile_keys):
        errors.append(
            "rig.json: unresolved Device Profile "
            f"{profile_key[0]!r}/{profile_key[1]!r}"
        )
    for profile_key in sorted(actual_profile_keys - expected_profile_keys):
        path = profiles_by_key[profile_key][0]
        errors.append(
            f"{path}: Device Profile {profile_key[0]!r}/{profile_key[1]!r} "
            "is not declared by rig.json"
        )

    selected_keys = [
        (slot["id"], slot["available_device_profiles"][0])
        for slot in rig["device_slots"]
        if len(slot["available_device_profiles"]) == 1
    ]
    if len(selected_keys) == len(rig["device_slots"]) and all(
        key in profiles_by_key for key in selected_keys
    ):
        errors.extend(
            validate_composed_ownership(
                [profiles_by_key[key] for key in selected_keys],
                "current Device Profile",
            )
        )

    return errors


def validate_composed_ownership(
    profile_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
    composition_label: str,
) -> List[str]:
    """Reject exclusive ownership shared by distinct composed profiles."""
    errors = []
    claims: Dict[Tuple[str, str], List[Tuple[Path, str]]] = defaultdict(list)
    for path, profile in profile_entries:
        for claim in profile["ownership"]:
            claims[(claim["kind"], claim["target"])].append(
                (path, claim["mode"])
            )
    for claim_key, claim_entries in claims.items():
        profile_paths = {path for path, _ in claim_entries}
        if len(profile_paths) > 1 and any(
            mode == "exclusive" for _, mode in claim_entries
        ):
            rendered = ", ".join(
                f"{path} ({mode})" for path, mode in claim_entries
            )
            errors.append(
                f"{composition_label} ownership conflict for "
                f"{claim_key[0]!r} target {claim_key[1]!r}: {rendered}"
            )
    return errors


def validate_rig_profile_catalogue_documents(
    rig: Mapping[str, Any],
    device_profile_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
    rig_profile_entries: Sequence[Tuple[Path, Mapping[str, Any]]],
) -> List[str]:
    """Resolve global compositions and validate their aggregate contracts."""
    errors = []
    slots_by_id = {slot["id"]: slot for slot in rig["device_slots"]}
    device_profiles_by_key = {
        (profile["slot"], profile["id"]): (path, profile)
        for path, profile in device_profile_entries
    }
    rig_profiles_by_id: Dict[str, Tuple[Path, Mapping[str, Any]]] = {}

    for path, profile in rig_profile_entries:
        profile_id = profile["id"]
        previous = rig_profiles_by_id.get(profile_id)
        if previous is not None:
            errors.append(
                f"{path}: duplicate Rig Profile {profile_id!r}; "
                f"already defined by {previous[0]}"
            )
        else:
            rig_profiles_by_id[profile_id] = (path, profile)
        if path.stem != profile_id or path.parent.name != "rig-profiles":
            errors.append(
                f"{path}: file layout must be rig-profiles/{profile_id}.json"
            )

    expected_ids = set(rig["rig_profiles"])
    actual_ids = set(rig_profiles_by_id)
    for profile_id in sorted(expected_ids - actual_ids):
        errors.append(f"rig.json: unresolved Rig Profile {profile_id!r}")
    for profile_id in sorted(actual_ids - expected_ids):
        path = rig_profiles_by_id[profile_id][0]
        errors.append(
            f"{path}: Rig Profile {profile_id!r} is not declared by rig.json"
        )

    for field in ("default_rig_profile", "fallback_rig_profile"):
        profile_id = rig.get(field)
        if profile_id is not None and profile_id not in expected_ids:
            errors.append(
                f"rig.json: {field} {profile_id!r} is not declared in "
                "rig_profiles"
            )

    readiness_rank = {"control-only": 0, "prepared": 1, "cold": 2}
    dependency_kinds = {
        "engines": "engine",
        "effects": "effect",
        "routes": "route",
    }
    for path, rig_profile in rig_profile_entries:
        selected_entries = []
        selected_slots = set(rig_profile["device_profiles"])
        for slot_id, device_profile_id in rig_profile["device_profiles"].items():
            slot = slots_by_id.get(slot_id)
            if slot is None:
                errors.append(
                    f"{path}: unresolved device slot {slot_id!r}"
                )
                continue
            if device_profile_id not in slot["available_device_profiles"]:
                errors.append(
                    f"{path}: Device Profile {slot_id!r}/{device_profile_id!r} "
                    "is not available from rig.json"
                )
                continue
            selected = device_profiles_by_key.get((slot_id, device_profile_id))
            if selected is None:
                errors.append(
                    f"{path}: unresolved Device Profile "
                    f"{slot_id!r}/{device_profile_id!r}"
                )
                continue
            selected_entries.append(selected)

        for slot in rig["device_slots"]:
            if slot.get("required", True) and slot["id"] not in selected_slots:
                errors.append(
                    f"{path}: required slot {slot['id']!r} is not selected"
                )

        selected_capabilities = {
            capability
            for _, profile in selected_entries
            for capability in profile["required_capabilities"]
        }
        selected_capabilities.update(
            endpoint
            for _, profile in selected_entries
            for endpoint in slots_by_id[profile["slot"]]["required_endpoints"]
        )
        missing_capabilities = selected_capabilities - set(
            rig_profile["required_capabilities"]
        )
        if missing_capabilities:
            errors.append(
                f"{path}: required_capabilities omit selected Device Profile "
                f"requirements {sorted(missing_capabilities)}"
            )

        selected_readiness = max(
            (readiness_rank[profile["readiness"]] for _, profile in selected_entries),
            default=0,
        )
        if readiness_rank[rig_profile["readiness"]] < selected_readiness:
            errors.append(
                f"{path}: readiness {rig_profile['readiness']!r} understates "
                "a selected Device Profile"
            )

        errors.extend(
            validate_composed_ownership(
                selected_entries + [(path, rig_profile)],
                f"Rig Profile {rig_profile['id']!r}",
            )
        )

        state_keys = {
            claim["target"]
            for _, profile in selected_entries + [(path, rig_profile)]
            for claim in profile["ownership"]
            if claim["kind"] == "state-key"
        }
        for state_key in rig_profile.get("initial_state", {}):
            if state_key not in state_keys:
                errors.append(
                    f"{path}: initial state key {state_key!r} has no "
                    "state-key ownership claim"
                )

        dependency_counts: Dict[Tuple[str, str], int] = defaultdict(int)
        available_resources = set()
        for _, profile in selected_entries:
            for category, resources in profile["dependencies"].items():
                for resource in resources:
                    available_resources.add(resource)
                    if category in dependency_kinds:
                        dependency_counts[(category, resource)] += 1

        profile_ownership = {
            (claim["kind"], claim["target"])
            for claim in rig_profile["ownership"]
        }
        for category, resource_kind in dependency_kinds.items():
            declared = set(rig_profile.get("shared_resources", {}).get(category, []))
            required_shared = {
                resource
                for (resource_category, resource), count in dependency_counts.items()
                if resource_category == category and count > 1
            }
            missing_shared = required_shared - declared
            if missing_shared:
                errors.append(
                    f"{path}: shared_resources.{category} omit selected "
                    f"resources {sorted(missing_shared)}"
                )
            for resource in sorted(declared):
                if (
                    (category, resource) not in dependency_counts
                    and (resource_kind, resource) not in profile_ownership
                ):
                    errors.append(
                        f"{path}: shared resource {resource!r} is neither a "
                        "selected dependency nor owned by the Rig Profile"
                    )

        pin_targets = available_resources | set(rig_profile["required_capabilities"])
        unknown_pins = set(
            rig_profile["preparation"]["pinned_capabilities"]
        ) - pin_targets
        if unknown_pins:
            errors.append(
                f"{path}: pinned_capabilities contain unresolved targets "
                f"{sorted(unknown_pins)}"
            )

    return errors


def validate_authored_catalogue(
    rig_root: Path,
    schemas: Mapping[str, Mapping[str, Any]],
    registry: Registry,
) -> Tuple[List[str], int, int, int, int]:
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

    profile_entries = []
    for profile_path in sorted(
        (rig_root / "device-profiles").glob("*/*.json")
    ):
        profile = load_json(profile_path)
        profile_kind, profile_errors = validate_document(
            profile, schemas, registry
        )
        errors.extend(f"{profile_path}: {error}" for error in profile_errors)
        if profile_kind != "device-profile":
            errors.append(f"{profile_path}: expected a device-profile document")
        if not profile_errors and profile_kind == "device-profile":
            profile_entries.append((profile_path, profile))

    rig_profile_entries = []
    for profile_path in sorted((rig_root / "rig-profiles").glob("*.json")):
        profile = load_json(profile_path)
        profile_kind, profile_errors = validate_document(
            profile, schemas, registry
        )
        errors.extend(f"{profile_path}: {error}" for error in profile_errors)
        if profile_kind != "rig-profile":
            errors.append(f"{profile_path}: expected a rig-profile document")
        if not profile_errors and profile_kind == "rig-profile":
            rig_profile_entries.append((profile_path, profile))

    if errors:
        return errors, 0, 0, 0, 0
    semantic_errors, verified_count, partial_count = (
        validate_hardware_catalogue_documents(rig, preset_entries)
    )
    semantic_errors.extend(
        validate_device_profile_catalogue_documents(
            rig, preset_entries, profile_entries
        )
    )
    semantic_errors.extend(
        validate_rig_profile_catalogue_documents(
            rig, profile_entries, rig_profile_entries
        )
    )
    return (
        semantic_errors,
        verified_count,
        partial_count,
        len(profile_entries),
        len(rig_profile_entries),
    )


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


def validate_current_pad_extraction(
    rig_root: Path,
    capture_path: Path,
) -> List[str]:
    """Compare the two pad presets with the raw Airstar capture evidence."""
    capture = load_json(capture_path)
    errors = []
    expected_schema = "music-studies/hardware-preset-capture/v1"
    if capture.get("schema") != expected_schema:
        return [f"{capture_path}: expected schema {expected_schema!r}"]

    safety = capture.get("safety", {})
    pre_fingerprint = safety.get("pre_subscription_sha256")
    post_fingerprint = safety.get("post_subscription_sha256")
    if safety.get("temporary_subscription_only") is not True:
        errors.append(f"{capture_path}: capture was not subscription-only")
    if (
        not isinstance(pre_fingerprint, str)
        or len(pre_fingerprint) != 64
        or pre_fingerprint != post_fingerprint
    ):
        errors.append(
            f"{capture_path}: valid matching subscription fingerprints missing"
        )
    if safety.get("remaining_observers") != 0:
        errors.append(f"{capture_path}: temporary observers remain")
    if safety.get("operator_confirmed_audio_after_cleanup") is not True:
        errors.append(
            f"{capture_path}: post-cleanup operator audio confirmation missing"
        )

    source_ref = capture.get("source_ref")
    device_entries = capture.get("devices")
    if not isinstance(source_ref, str) or not source_ref:
        errors.append(f"{capture_path}: source_ref is missing")
    if not isinstance(device_entries, list):
        errors.append(f"{capture_path}: devices must be an array")
        return errors

    expected_preset_ids = {
        "smc-pad-current-notes",
        "smc-pad-pocket-current-notes",
    }
    entries_by_id = {
        entry.get("hardware_preset"): entry
        for entry in device_entries
        if isinstance(entry, dict)
    }
    if (
        len(entries_by_id) != len(device_entries)
        or set(entries_by_id) != expected_preset_ids
    ):
        errors.append(
            f"{capture_path}: pad preset evidence mismatch; "
            f"expected={sorted(expected_preset_ids)} "
            f"actual={sorted(str(item) for item in entries_by_id)}"
        )
        return errors

    for preset_id in sorted(expected_preset_ids):
        entry = entries_by_id[preset_id]
        preset_path = rig_root / "hardware-presets" / f"{preset_id}.json"
        preset = load_json(preset_path)
        if preset["verification"]["status"] != "verified":
            errors.append(f"{preset_path}: live pad extraction must be verified")
        if source_ref not in preset["verification"]["source_refs"]:
            errors.append(
                f"{preset_path}: missing capture source_ref {source_ref!r}"
            )

        expected_controls = {
            pad["id"]: {
                "behavior": "momentary",
                "type": "note",
                "channel": entry["midi_channel"],
                "number": pad["note"],
            }
            for pad in entry["pads"]
        }
        raw_note_on_sequence = entry.get("raw_note_on_sequence", [])
        if not all(
            pad["note"] in raw_note_on_sequence for pad in entry["pads"]
        ):
            errors.append(
                f"{capture_path}: raw note sequence does not cover every "
                f"mapped pad for {preset_id!r}"
            )
        if entry.get("silent_control_press_count") != len(
            entry["internal_controls"]
        ):
            errors.append(
                f"{capture_path}: silent control count differs from "
                f"internal controls for {preset_id!r}"
            )
        actual_controls = {
            control["id"]: control_signature(control)
            for control in preset["controls"]
            if control["id"].startswith("performance-pad-")
        }
        if actual_controls != expected_controls:
            errors.append(
                f"{preset_path}: pad controls differ from live capture; "
                f"expected={expected_controls} actual={actual_controls}"
            )

        expected_internal = {
            control["id"]: control["action"]
            for control in entry["internal_controls"]
        }
        actual_internal = {
            control["id"]: control["action"]
            for control in preset.get("internal_controls", [])
        }
        if actual_internal != expected_internal:
            errors.append(
                f"{preset_path}: internal controls differ from live capture; "
                f"expected={expected_internal} actual={actual_internal}"
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
    fixture_profile_path = Path(
        "device-profiles/fixture-controller/fixture-role.json"
    )
    fixture_profile = load_json(
        fixture_dir / "valid" / "device-profile.json"
    )
    fixture_rig_profile_path = Path("rig-profiles/fixture-live.json")
    fixture_rig_profile = load_json(
        fixture_dir / "valid" / "rig-profile.json"
    )
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

    catalogue_errors = validate_device_profile_catalogue_documents(
        fixture_rig,
        [(fixture_preset_path, fixture_preset)],
        [(fixture_profile_path, fixture_profile)],
    )
    if catalogue_errors:
        failures.append(
            "valid Device Profile catalogue failed: "
            f"{'; '.join(catalogue_errors)}"
        )

    catalogue_errors = validate_rig_profile_catalogue_documents(
        fixture_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, fixture_rig_profile)],
    )
    if catalogue_errors:
        failures.append(
            "valid Rig Profile catalogue failed: "
            f"{'; '.join(catalogue_errors)}"
        )

    unresolved_rig_profile_rig = json.loads(json.dumps(fixture_rig))
    unresolved_rig_profile_rig["rig_profiles"] = ["missing-rig-profile"]
    catalogue_errors = validate_rig_profile_catalogue_documents(
        unresolved_rig_profile_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, fixture_rig_profile)],
    )
    if not any("unresolved Rig Profile" in error for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject an unresolved "
            "global profile"
        )

    missing_slot_rig_profile = json.loads(json.dumps(fixture_rig_profile))
    missing_slot_rig_profile["device_profiles"] = {}
    catalogue_errors = validate_rig_profile_catalogue_documents(
        fixture_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, missing_slot_rig_profile)],
    )
    if not any("required slot" in error for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject a missing required "
            "slot"
        )

    unresolved_selection_rig = json.loads(json.dumps(fixture_rig))
    unresolved_selection_rig["device_slots"][0][
        "available_device_profiles"
    ] = ["missing-role"]
    unresolved_selection_profile = json.loads(json.dumps(fixture_rig_profile))
    unresolved_selection_profile["device_profiles"][
        "fixture-controller"
    ] = "missing-role"
    catalogue_errors = validate_rig_profile_catalogue_documents(
        unresolved_selection_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, unresolved_selection_profile)],
    )
    if not any("unresolved Device Profile" in error for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject an unresolved "
            "Device Profile selection"
        )

    missing_capability_profile = json.loads(json.dumps(fixture_rig_profile))
    missing_capability_profile["required_capabilities"] = [
        "midi.performance-input"
    ]
    catalogue_errors = validate_rig_profile_catalogue_documents(
        fixture_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, missing_capability_profile)],
    )
    if not any("required_capabilities omit" in error for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject an omitted selected "
            "capability"
        )

    unowned_state_profile = json.loads(json.dumps(fixture_rig_profile))
    unowned_state_profile["initial_state"]["unowned.state"] = True
    catalogue_errors = validate_rig_profile_catalogue_documents(
        fixture_rig,
        [(fixture_profile_path, fixture_profile)],
        [(fixture_rig_profile_path, unowned_state_profile)],
    )
    if not any("initial state key" in error for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject unowned initial state"
        )

    unresolved_profile = dict(fixture_profile)
    unresolved_profile["hardware_preset"] = "missing-preset"
    catalogue_errors = validate_device_profile_catalogue_documents(
        fixture_rig,
        [(fixture_preset_path, fixture_preset)],
        [(fixture_profile_path, unresolved_profile)],
    )
    if not any("unresolved hardware preset" in error for error in catalogue_errors):
        failures.append(
            "invalid Device Profile catalogue did not reject an unresolved "
            "Hardware Preset"
        )

    missing_control_profile = json.loads(json.dumps(fixture_profile))
    missing_control_profile["mappings"][0]["source_control"] = "missing-control"
    catalogue_errors = validate_device_profile_catalogue_documents(
        fixture_rig,
        [(fixture_preset_path, fixture_preset)],
        [(fixture_profile_path, missing_control_profile)],
    )
    if not any(
        "is not defined by hardware preset" in error
        for error in catalogue_errors
    ):
        failures.append(
            "invalid Device Profile catalogue did not reject an unresolved "
            "source control"
        )

    conflict_rig = json.loads(json.dumps(fixture_rig))
    conflict_slot = json.loads(json.dumps(conflict_rig["device_slots"][0]))
    conflict_slot["id"] = "fixture-controller-two"
    conflict_slot["display_name"] = "Fixture Controller Two"
    conflict_slot["available_device_profiles"] = ["fixture-role-two"]
    for selector in conflict_slot["selectors"]:
        if selector["kind"] == "local-discriminator":
            selector["value"] = "fixture-controller-two"
    conflict_rig["device_slots"].append(conflict_slot)
    conflict_profile = json.loads(json.dumps(fixture_profile))
    conflict_profile["id"] = "fixture-role-two"
    conflict_profile["slot"] = "fixture-controller-two"
    conflict_profile_path = Path(
        "device-profiles/fixture-controller-two/fixture-role-two.json"
    )
    catalogue_errors = validate_device_profile_catalogue_documents(
        conflict_rig,
        [(fixture_preset_path, fixture_preset)],
        [
            (fixture_profile_path, fixture_profile),
            (conflict_profile_path, conflict_profile),
        ],
    )
    if not any("ownership conflict" in error for error in catalogue_errors):
        failures.append(
            "invalid Device Profile catalogue did not reject exclusive "
            "ownership overlap"
        )

    conflict_rig_profile = json.loads(json.dumps(fixture_rig_profile))
    conflict_rig_profile["device_profiles"][
        "fixture-controller-two"
    ] = "fixture-role-two"
    catalogue_errors = validate_rig_profile_catalogue_documents(
        conflict_rig,
        [
            (fixture_profile_path, fixture_profile),
            (conflict_profile_path, conflict_profile),
        ],
        [(fixture_rig_profile_path, conflict_rig_profile)],
    )
    if not any("Rig Profile" in error and "ownership conflict" in error
               for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject exclusive ownership "
            "overlap"
        )

    missing_shared_profile = json.loads(json.dumps(conflict_rig_profile))
    missing_shared_profile["shared_resources"]["engines"] = []
    catalogue_errors = validate_rig_profile_catalogue_documents(
        conflict_rig,
        [
            (fixture_profile_path, fixture_profile),
            (conflict_profile_path, conflict_profile),
        ],
        [(fixture_rig_profile_path, missing_shared_profile)],
    )
    if not any("shared_resources.engines omit" in error
               for error in catalogue_errors):
        failures.append(
            "invalid Rig Profile catalogue did not reject an undeclared "
            "shared dependency"
        )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print(
        "Performance Rig schemas: PASS "
        f"(schemas={len(schemas)}, valid={valid_count}, invalid={invalid_count}, "
        "catalogues=14)"
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
    authority_pad_capture: Optional[Path],
) -> int:
    schemas = load_schemas(schema_dir)
    registry = build_registry(schemas)
    errors, verified_count, partial_count, profile_count, rig_profile_count = (
        validate_authored_catalogue(
            rig_root,
            schemas,
            registry,
        )
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
    if authority_pad_capture is not None:
        extraction_errors = validate_current_pad_extraction(
            rig_root,
            authority_pad_capture,
        )
        if extraction_errors:
            for error in extraction_errors:
                print(error, file=sys.stderr)
            return 1
    print(
        f"{rig_root}: OK (hardware-presets="
        f"{verified_count + partial_count}, verified={verified_count}, "
        f"partial={partial_count}, device-profiles={profile_count}, "
        f"rig-profiles={rig_profile_count})"
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
    parser.add_argument(
        "--authority-pad-capture",
        type=Path,
        help="live pad-capture JSON used for current-preset extraction parity",
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
        help="validate the authored Rig, Hardware Presets, and profiles",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        if (
            arguments.authority_setup is not None
            or arguments.authority_pad_capture is not None
        ) and not arguments.validate_root:
            raise ValueError("authority evidence requires --validate-root")
        if arguments.self_test:
            return run_self_test(arguments.schema_dir, arguments.fixture_dir)
        if arguments.validate_root:
            return run_root_validation(
                arguments.schema_dir,
                arguments.validate_root,
                arguments.authority_setup,
                arguments.authority_pad_capture,
            )
        return run_validation(arguments.schema_dir, arguments.validate)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
