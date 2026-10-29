#!/usr/bin/env python3
"""Check USB MIDI route recovery without PipeWire hardware."""

from __future__ import annotations

from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader
import sys
import unittest
from unittest.mock import patch


def load_selector(path):
    loader = SourceFileLoader("music_rig_arturia_selector", str(path))
    spec = spec_from_loader(loader.name, loader)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load Arturia selector: {path}")
    module = module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class IndependentDeviceHotplugRecoveryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.selector = load_selector(sys.argv.pop(1))

    def test_reconnects_reenumerated_smk25_capture_port(self):
        selector = self.selector
        alias = "SMK25-Master"
        source = "Midi-Bridge:SINCO 9:(capture_1) SINCO " + alias
        targets = selector.INDEPENDENT_DEVICE_MIDI_TARGETS[alias]
        responses = {"-o": source + "\n", "-i": targets[0] + "\n"}

        def run(command, environment):
            return selector.subprocess.CompletedProcess(
                command, 0, responses[command[-1]], ""
            )

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links", return_value=""), \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_independent_device_midi_routes({})

        self.assertEqual([(alias, source, targets[0])], repaired)
        connect.assert_called_once_with(source, targets[0], {})

    def test_reconnects_smc_pad_and_pocket_capture_ports(self):
        selector = self.selector
        pad_aliases = ("SMC-PAD-Master", "SMC-PAD Pocket-Master")
        sources = {
            alias: f"Midi-Bridge:SINCO 9:(capture_1) SINCO {alias}"
            for alias in pad_aliases
        }
        targets = selector.INDEPENDENT_DEVICE_MIDI_TARGETS
        output_ports = "\n".join(sources.values())
        input_ports = "\n".join(
            target for alias in pad_aliases for target in targets[alias]
        )

        def run(command, environment):
            text = output_ports if command[-1] == "-o" else input_ports
            return selector.subprocess.CompletedProcess(command, 0, text, "")

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links", return_value=""), \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_independent_device_midi_routes({})

        expected = {
            (alias, sources[alias], target)
            for alias in pad_aliases for target in targets[alias]
        }
        self.assertEqual(expected, set(repaired))
        self.assertEqual(len(expected), connect.call_count)

    def test_existing_controller_routes_are_left_untouched(self):
        selector = self.selector
        routes = []
        outputs = []
        inputs = []
        for alias, targets in selector.INDEPENDENT_DEVICE_MIDI_TARGETS.items():
            source = f"Midi-Bridge:SINCO 3:(capture_1) SINCO {alias}"
            outputs.append(source)
            for target in targets:
                inputs.append(target)
                routes.append(source + "\n  |-> " + target + "\n")
        responses = {"-o": "\n".join(outputs), "-i": "\n".join(inputs)}

        def run(command, environment):
            return selector.subprocess.CompletedProcess(
                command, 0, responses[command[-1]], ""
            )

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links", return_value="".join(routes)), \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_independent_device_midi_routes({})

        self.assertEqual([], repaired)
        connect.assert_not_called()

    def test_absent_controller_ports_are_not_treated_as_an_error(self):
        selector = self.selector
        commands = []

        def run(command, environment):
            commands.append(command)
            output = "" if command[-1] == "-o" else "SMK25 Pad Layers:midi-in\n"
            return selector.subprocess.CompletedProcess(command, 0, output, "")

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links") as links, \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_independent_device_midi_routes({})

        self.assertEqual([], repaired)
        self.assertEqual([["pw-link", "-o"]], commands)
        links.assert_not_called()
        connect.assert_not_called()


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
