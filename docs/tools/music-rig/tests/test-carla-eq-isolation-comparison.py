#!/usr/bin/env python3
"""Contract checks for the device-free EQ report comparator."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[4]
SCRIPT = Path(__file__).resolve().parents[1] / "benchmarks/compare-carla-eq-isolation.py"
SPEC = importlib.util.spec_from_file_location("eq_compare", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def report(energy=100.0, events=128, invalid=0, mapping=0, over=0):
    scenario = {
        "name": "audio_midi",
        "midi_events": events,
        "mapping_errors": mapping,
        "invalid_audio_samples": invalid,
        "output_energy": energy,
        "wall": {"mean_us": 10.0, "p99_us": 20.0,
                 "over_quantum_blocks": over},
        "thread_cpu": {"mean_us": 8.0, "p99_us": 16.0,
                        "over_quantum_blocks": over},
    }
    return {
        "schema": MODULE.SCHEMA,
        "sha256": {"lsp_binary": "baseline", "lsp_metadata": "metadata"},
        "measurement": {"sample_rate": 48000, "frames": 1024,
                         "blocks_per_scenario": 128, "warmup_blocks": 8,
                         "scenarios": [scenario]},
    }


class ComparisonTests(unittest.TestCase):
    def write(self, directory, name, value):
        path = directory / name
        path.write_text(__import__("json").dumps(value))
        return path

    def run_comparison(self, baseline, candidate, tolerance=0.02):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            baseline_path = self.write(directory, "baseline.json", baseline)
            candidate_path = self.write(directory, "candidate.json", candidate)
            output = directory / "comparison.json"
            class Args:
                pass
            args = Args()
            args.baseline = baseline_path
            args.candidate = candidate_path
            args.output = output
            args.max_energy_relative_delta = tolerance
            MODULE.run(args)
            return __import__("json").loads(output.read_text())

    def test_parity_and_tolerance_pass(self):
        result = self.run_comparison(report(), report(101.0))
        self.assertTrue(result["acceptance"]["passed"])
        self.assertEqual(result["scenarios"][0]["midi_events"], 128)

    def test_energy_tolerance_rejects(self):
        with self.assertRaises(SystemExit):
            self.run_comparison(report(), report(103.0))

    def test_mapping_and_finite_audio_reject(self):
        with self.assertRaises(SystemExit):
            self.run_comparison(report(), report(mapping=1))
        with self.assertRaises(SystemExit):
            self.run_comparison(report(), report(invalid=1))

    def test_realtime_over_quantum_rejects(self):
        with self.assertRaises(SystemExit):
            self.run_comparison(report(), report(over=1))


if __name__ == "__main__":
    unittest.main()
