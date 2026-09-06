#!/usr/bin/env python3
"""Compare two device-free Carla EQ isolation reports."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path


SCHEMA = "music-studies/carla-eq-parameter-isolation/v1"


def load_report(path):
    report = json.loads(path.read_text())
    if report.get("schema") != SCHEMA:
        raise ValueError(f"{path}: unexpected report schema")
    measurement = report.get("measurement", {})
    scenarios = measurement.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        raise ValueError(f"{path}: missing scenario measurements")
    return report


def scenario_map(report, label):
    scenarios = report["measurement"]["scenarios"]
    result = {}
    for scenario in scenarios:
        name = scenario.get("name")
        if not isinstance(name, str) or name in result:
            raise ValueError(f"{label}: duplicate or invalid scenario name")
        result[name] = scenario
    return result


def relative_delta(left, right):
    denominator = max(abs(left), 1.0e-30)
    return abs(right - left) / denominator


def run(args):
    baseline_path = args.baseline.resolve(strict=True)
    candidate_path = args.candidate.resolve(strict=True)
    baseline = load_report(baseline_path)
    candidate = load_report(candidate_path)
    baseline_scenarios = scenario_map(baseline, "baseline")
    candidate_scenarios = scenario_map(candidate, "candidate")
    if set(baseline_scenarios) != set(candidate_scenarios):
        raise ValueError("baseline and candidate scenario sets differ")

    baseline_measurement = baseline["measurement"]
    candidate_measurement = candidate["measurement"]
    for key in ("sample_rate", "frames", "blocks_per_scenario", "warmup_blocks"):
        if baseline_measurement.get(key) != candidate_measurement.get(key):
            raise ValueError(f"measurement setting differs: {key}")

    rows = []
    mapping_parity = True
    finite_audio = True
    event_parity = True
    energy_parity = True
    candidate_realtime_safe = True
    for name in baseline_scenarios:
        left = baseline_scenarios[name]
        right = candidate_scenarios[name]
        same_events = left.get("midi_events") == right.get("midi_events")
        same_mapping = (left.get("mapping_errors") == 0 and
                        right.get("mapping_errors") == 0)
        finite = (left.get("invalid_audio_samples") == 0 and
                  right.get("invalid_audio_samples") == 0)
        energy_delta = relative_delta(left["output_energy"], right["output_energy"])
        energy_ok = energy_delta <= args.max_energy_relative_delta
        realtime_safe = all(
            right[metric]["over_quantum_blocks"] == 0
            for metric in ("wall", "thread_cpu")
        )
        mapping_parity &= same_mapping
        finite_audio &= finite
        event_parity &= same_events
        energy_parity &= energy_ok
        candidate_realtime_safe &= realtime_safe
        rows.append({
            "name": name,
            "midi_events": right["midi_events"],
            "energy_relative_delta": energy_delta,
            "energy_within_tolerance": energy_ok,
            "baseline_thread_cpu_mean_us": left["thread_cpu"]["mean_us"],
            "candidate_thread_cpu_mean_us": right["thread_cpu"]["mean_us"],
            "thread_cpu_mean_speedup": (
                left["thread_cpu"]["mean_us"] /
                right["thread_cpu"]["mean_us"]),
            "baseline_thread_cpu_p99_us": left["thread_cpu"]["p99_us"],
            "candidate_thread_cpu_p99_us": right["thread_cpu"]["p99_us"],
            "thread_cpu_p99_speedup": (
                left["thread_cpu"]["p99_us"] /
                right["thread_cpu"]["p99_us"]),
        })

    result = {
        "schema": "music-studies/carla-eq-isolation-comparison/v1",
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "baseline_report": str(baseline_path),
        "candidate_report": str(candidate_path),
        "baseline": {
            "lsp_binary_sha256": baseline["sha256"]["lsp_binary"],
            "lsp_metadata_sha256": baseline["sha256"]["lsp_metadata"],
        },
        "candidate": {
            "lsp_binary_sha256": candidate["sha256"]["lsp_binary"],
            "lsp_metadata_sha256": candidate["sha256"]["lsp_metadata"],
        },
        "measurement": {
            key: baseline_measurement[key]
            for key in ("sample_rate", "frames", "blocks_per_scenario", "warmup_blocks")
        },
        "acceptance": {
            "mapping_parity": mapping_parity,
            "finite_audio": finite_audio,
            "event_parity": event_parity,
            "energy_within_tolerance": energy_parity,
            "candidate_zero_over_quantum_blocks": candidate_realtime_safe,
            "max_energy_relative_delta": args.max_energy_relative_delta,
            "passed": (mapping_parity and finite_audio and event_parity and
                       energy_parity and candidate_realtime_safe),
        },
        "scenarios": rows,
        "limitations": [
            "This compares device-free plugin blocks, not live JACK or PipeWire scheduling.",
            "Energy parity is a coarse response check; it is not an impulse or frequency-response match.",
            "The candidate uses newer upstream LSP code and is not the protected production plugin.",
        ],
    }
    output = args.output.resolve()
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(output)
    if not result["acceptance"]["passed"]:
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-energy-relative-delta", type=float, default=0.02)
    args = parser.parse_args()
    if args.max_energy_relative_delta < 0:
        parser.error("--max-energy-relative-delta must not be negative")
    if args.output.exists():
        parser.error("--output must not already exist")
    run(args)


if __name__ == "__main__":
    main()
