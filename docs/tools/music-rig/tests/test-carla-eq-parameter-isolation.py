#!/usr/bin/env python3
"""Contract checks without loading Carla, plugins, audio servers, or hardware."""

import copy
import importlib.util
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[4]
RUNNER = Path(__file__).resolve().parents[1] / "benchmarks/run-carla-eq-parameter-isolation.py"
SPEC = importlib.util.spec_from_file_location("eq_isolation", RUNNER)
MODULE = importlib.util.module_from_spec(SPEC)
sys.dont_write_bytecode = True
SPEC.loader.exec_module(MODULE)
PROJECT = ROOT / "src/audio-software/carla/projects/pedro-live-rack/pedro.uproject"


class ProjectIsolationTests(unittest.TestCase):
    def setUp(self):
        self.source = PROJECT.read_bytes()
        self.root = ET.fromstring(self.source)
        self.eq = next(p for p in self.root.findall("Plugin")
                       if p.findtext("Info/URI") == MODULE.EQ_URI)

    def isolate(self):
        return MODULE.isolated_project(ET.tostring(self.root))

    def test_only_eq_is_copied_without_engine_or_links(self):
        isolated = ET.fromstring(self.isolate())
        self.assertEqual([e.tag for e in isolated], ["Plugin"])
        self.assertEqual(isolated.findtext("Plugin/Info/URI"), MODULE.EQ_URI)

    def test_parameter_values_and_mappings_are_preserved(self):
        copied = ET.fromstring(self.isolate()).find("Plugin")
        def contents(plugin):
            return [{field.tag: field.text for field in parameter}
                    for parameter in plugin.findall("Data/Parameter")]
        self.assertEqual(contents(self.eq), contents(copied))
        self.assertEqual(copied.findtext("Data/Options"), "0x1")
        self.assertEqual(PROJECT.read_bytes(), self.source)

    def test_missing_eq_rejected(self):
        self.root.remove(self.eq)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.isolate()

    def test_duplicate_eq_rejected(self):
        self.root.append(copy.deepcopy(self.eq))
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.isolate()

    def test_inactive_eq_rejected(self):
        self.eq.find("Data/Active").text = "No"
        with self.assertRaisesRegex(ValueError, "active LV2"):
            self.isolate()

    def test_unexpected_buffer_options_rejected(self):
        self.eq.find("Data/Options").text = "0x0"
        with self.assertRaisesRegex(ValueError, "options"):
            self.isolate()

    def test_missing_or_duplicate_gain_rejected(self):
        data = self.eq.find("Data")
        gain = next(p for p in data.findall("Parameter") if p.findtext("Symbol") == "g_0")
        data.append(copy.deepcopy(gain))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.isolate()
        data.remove(gain)
        data.remove(next(p for p in data.findall("Parameter") if p.findtext("Symbol") == "g_0"))
        with self.assertRaisesRegex(ValueError, "missing"):
            self.isolate()

    def test_wrong_channel_or_cc_rejected(self):
        gain = next(p for p in self.eq.findall("Data/Parameter") if p.findtext("Symbol") == "g_0")
        for field, wrong in (("MidiChannel", "2"), ("MappedControlIndex", "40")):
            with self.subTest(field=field):
                node = gain.find(field)
                previous = node.text
                node.text = wrong
                with self.assertRaisesRegex(ValueError, "MIDI mapping"):
                    self.isolate()
                node.text = previous

    def test_invalid_gain_ranges_rejected(self):
        gain = next(p for p in self.eq.findall("Data/Parameter") if p.findtext("Symbol") == "g_0")
        for field, value in (("MappedMinimum", "nan"), ("MappedMinimum", "0"),
                             ("MappedMaximum", "inf"), ("MappedMaximum", "0.5")):
            with self.subTest(field=field, value=value):
                node = gain.find(field)
                previous = node.text
                node.text = value
                with self.assertRaisesRegex(ValueError, "mapping range"):
                    self.isolate()
                node.text = previous


class HostLogTests(unittest.TestCase):
    def test_empty_log_is_valid(self):
        self.assertEqual(MODULE.check_host_log("")["assertion_lines"], 0)

    def test_known_warning_is_retained(self):
        result = MODULE.check_host_log("WARNING - Broken plugin parameter 'Filter select': min >= max")
        self.assertEqual(result["known_filter_select_warnings"], 1)

    def test_runtime_failures_reject_evidence(self):
        for text in ("Carla assertion failure", "Assertion failed", "Segmentation fault",
                     "AddressSanitizer report", "runtime error: bad pointer", "Error opening file"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                MODULE.check_host_log(text)


class ProfileTests(unittest.TestCase):
    def test_clean_profile_remains_a_sampling_estimate(self):
        result = MODULE.profile_quality("Experiment: sample.er\nNo errors\n")
        self.assertTrue(result["quantitative_profile_valid"])
        self.assertEqual(result["interpretation"], "sampling estimates")

    def test_timer_warning_invalidates_precise_percentages(self):
        warning = "*** Collector Warning: Collection interval timer period was changed (10007 -> 0)"
        result = MODULE.profile_quality("No errors\n" + warning + "\n")
        self.assertFalse(result["quantitative_profile_valid"])
        self.assertEqual(result["warnings"], [warning])
        self.assertEqual(result["interpretation"], "qualitative stacks only")

    def test_system_warning_is_not_discarded(self):
        self.assertFalse(MODULE.profile_quality("*** Warning: variable clock frequency")
                         ["quantitative_profile_valid"])

    def test_profile_wraps_only_target_without_kernel_counters(self):
        target = ["/tmp/test target", "argument with spaces"]
        command = MODULE.profile_command(target, Path("/tmp/evidence"))
        self.assertEqual(command[-2:], target)
        self.assertIn("/tmp/evidence/profile.er", command)
        self.assertEqual(command[command.index("-F") + 1], "off")
        self.assertNotIn("-h", command)
        self.assertNotIn("-O", command)


if __name__ == "__main__":
    unittest.main()
