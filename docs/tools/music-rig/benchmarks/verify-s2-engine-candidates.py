#!/usr/bin/env python3
"""Validate isolated S2 engine discovery evidence or inspect staged plugins."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Mapping


DISCOVERY_FIELDS = {
    "audio.ins": "audio_inputs",
    "audio.outs": "audio_outputs",
    "midi.ins": "midi_inputs",
    "midi.outs": "midi_outputs",
    "parameters.ins": "parameter_inputs",
    "parameters.outs": "parameter_outputs",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return "sha256:" + digest.hexdigest()


def discovery(path: Path) -> Mapping[str, Any]:
    result = subprocess.run(
        ["/usr/lib/carla/carla-discovery-native", "lv2", str(path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise ValueError(f"Carla discovery failed for {path}: {result.stderr}")
    values: dict[str, Any] = {}
    for line in result.stdout.splitlines():
        prefix = "carla-discovery::"
        if not line.startswith(prefix) or "::" not in line[len(prefix):]:
            continue
        key, value = line[len(prefix):].split("::", 1)
        if key in DISCOVERY_FIELDS:
            values[DISCOVERY_FIELDS[key]] = int(value)
        elif key in {"name", "maker", "label", "category"}:
            values[key] = value
    required = set(DISCOVERY_FIELDS.values()) | {"name", "maker", "label"}
    if not required <= values.keys():
        raise ValueError(f"incomplete Carla discovery for {path}: {values}")
    return values


def lv2_parameters(path: Path) -> Mapping[str, Mapping[str, Any]]:
    text = (path / "dsp.ttl").read_text(encoding="utf-8")
    pattern = re.compile(
        r"plug:(?P<symbol>[^\s]+)\s+a lv2:Parameter ;(?P<body>.*?)(?=\n\s*plug:|\Z)",
        re.DOTALL,
    )
    parameters: dict[str, Mapping[str, Any]] = {}
    for match in pattern.finditer(text):
        body = match.group("body")
        label = re.search(r'rdfs:label "([^"]*)"', body)
        minimum = re.search(r"lv2:minimum ([^ ;]+)", body)
        maximum = re.search(r"lv2:maximum ([^ ;]+)", body)
        if label is None or minimum is None or maximum is None:
            raise ValueError(f"incomplete LV2 parameter {match.group('symbol')}")
        parameters[match.group("symbol")] = {
            "label": label.group(1),
            "minimum": float(minimum.group(1)),
            "maximum": float(maximum.group(1)),
        }
    if not parameters:
        raise ValueError(f"no LV2 parameters found in {path / 'dsp.ttl'}")
    return parameters


def setbfree_contract(binary: Path, bundle: Path, package: Path | None) -> Mapping[str, Any]:
    environment = os.environ.copy()
    library_root = binary.parents[1] / "lib"
    environment["LD_LIBRARY_PATH"] = str(library_root)
    result = subprocess.run(
        [str(binary), "-H"],
        env=environment,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise ValueError(f"setBfree help failed: {result.stderr}")
    help_text = result.stdout
    required_fragments = (
        "midi.controller.upper.<cc>",
        "drawbar<NUM>",
        "range is inversely mapped",
        "quantized into 0 ... 8",
    )
    missing = [fragment for fragment in required_fragments if fragment not in help_text]
    if missing:
        raise ValueError(f"setBfree control contract omitted {missing}")
    plugin = discovery(bundle)
    expected = {
        "audio_inputs": 0,
        "audio_outputs": 2,
        "midi_inputs": 1,
        "midi_outputs": 1,
        "parameter_inputs": 0,
        "parameter_outputs": 0,
    }
    if any(plugin.get(key) != value for key, value in expected.items()):
        raise ValueError(f"unexpected setBfree discovery: {plugin}")
    result_data: dict[str, Any] = {
        "discovery": plugin,
        "control_surface": "midi-cc",
        "drawbar_function_numbers": [16, 513, 8, 4, 223, 2, 135, 113, 1],
        "drawbar_range": [0, 127],
        "drawbar_positions": 9,
        "drawbar_inverted": True,
        "state_restore": "documented-not-runtime-tested",
        "host_parameter_binding": "unavailable",
        "plugin_uri": "http://gareus.org/oss/lv2/b_synth",
        "lv2_ttl": sha256(bundle / "b_synth.ttl"),
    }
    if package is not None:
        result_data["package_sha256"] = sha256(package)
    return result_data


def surge_contract(bundle: Path, package: Path | None) -> Mapping[str, Any]:
    plugin = discovery(bundle)
    expected = {
        "audio_inputs": 2,
        "audio_outputs": 6,
        "midi_inputs": 1,
        "midi_outputs": 0,
        "parameter_inputs": 1,
        "parameter_outputs": 0,
    }
    if any(plugin.get(key) != value for key, value in expected.items()):
        raise ValueError(f"unexpected Surge XT discovery: {plugin}")
    parameters = lv2_parameters(bundle)
    required = {
        "a_osc1_param3": "A Osc 1 Sub Mix",
        "a_level_o1": "A Osc 1 Volume",
        "a_filter1_cutoff": "A Filter 1 Cutoff",
        "a_filter1_resonance": "A Filter 1 Resonance",
        "a_env1_attack": "A Amp EG Attack",
        "a_env1_release": "A Amp EG Release",
        "a_lfo0_rate": "A LFO 1 Rate",
        "a_osc1_param6": "A Osc 1 Unison Voices",
        "a_ws_drive": "A Waveshaper Drive",
        "volume": "Global Volume",
    }
    for symbol, label in required.items():
        if parameters.get(symbol, {}).get("label") != label:
            raise ValueError(f"Surge XT parameter {symbol} is not stable")
    if any(
        value["minimum"] != 0.0 or value["maximum"] != 1.0
        for value in parameters.values()
    ):
        raise ValueError("Surge XT LV2 parameter range contract changed")
    result_data: dict[str, Any] = {
        "discovery": plugin,
        "parameter_count": len(parameters),
        "representative_parameters": required,
        "parameter_range": [0.0, 1.0],
        "state_restore": "LV2-preset-metadata-present-runtime-not-tested",
        "midi_control": "MIDI-input-present-host-automation-present",
        "plugin_uri": "https://surge-synthesizer.github.io/lv2/surge-xt",
        "lv2_dsp_ttl": sha256(bundle / "dsp.ttl"),
    }
    if package is not None:
        result_data["package_sha256"] = sha256(package)
    return result_data


def validate_evidence(document: Mapping[str, Any]) -> None:
    if document.get("schema") != "music-studies/s2-engine-inventory/v1":
        raise ValueError("invalid S2 engine inventory schema")
    candidates = document.get("candidates")
    if not isinstance(candidates, Mapping) or set(candidates) != {"setbfree", "surge-xt"}:
        raise ValueError("S2 inventory must contain both candidate engines")
    runtime_checks = document.get("runtime_checks")
    if not isinstance(runtime_checks, Mapping) or runtime_checks.get("status") != "blocked-by-safe-boundary":
        raise ValueError("S2 inventory must record the isolated runtime boundary")
    for candidate in candidates.values():
        if not isinstance(candidate, Mapping) or "discovery" not in candidate:
            raise ValueError("candidate inventory is incomplete")
    if candidates["setbfree"]["discovery"]["parameter_inputs"] != 0:
        raise ValueError("setBfree must remain MIDI-only in the inventory")
    if candidates["surge-xt"]["parameter_count"] < 700:
        raise ValueError("Surge XT parameter inventory is unexpectedly small")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--setbfree-binary", type=Path)
    parser.add_argument("--setbfree-bundle", type=Path)
    parser.add_argument("--setbfree-package", type=Path)
    parser.add_argument("--surge-bundle", type=Path)
    parser.add_argument("--surge-package", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.fixture is not None:
            validate_evidence(json.loads(arguments.fixture.read_text(encoding="utf-8")))
            print("S2 engine inventory fixture: PASS")
            return 0
        required = (
            arguments.setbfree_binary,
            arguments.setbfree_bundle,
            arguments.surge_bundle,
        )
        if any(value is None for value in required):
            parser.error("live verification requires both candidate paths")
        document = {
            "schema": "music-studies/s2-engine-inventory/v1",
            "status": "isolated discovery; no live rig activation",
            "candidates": {
                "setbfree": setbfree_contract(
                    arguments.setbfree_binary,
                    arguments.setbfree_bundle,
                    arguments.setbfree_package,
                ),
                "surge-xt": surge_contract(
                    arguments.surge_bundle,
                    arguments.surge_package,
                ),
            },
        }
        if arguments.output is not None:
            arguments.output.write_text(
                json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        print(json.dumps(document, ensure_ascii=True, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
