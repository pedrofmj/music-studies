#!/usr/bin/env python3
"""Check SMK-25 USB MIDI route recovery without PipeWire hardware."""

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


class Smk25HotplugRecoveryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.selector = load_selector(sys.argv.pop(1))

    def test_reconnects_reenumerated_smk25_capture_port(self):
        selector = self.selector
        source = "Midi-Bridge:SINCO 9:(capture_1) SINCO SMK25-Master"
        target = selector.SMK25_MIDI_INPUT
        responses = {"-o": source + "\n", "-i": target + "\n"}

        def run(command, environment):
            return selector.subprocess.CompletedProcess(
                command, 0, responses[command[-1]], ""
            )

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links", return_value=""), \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_smk25_midi_route({})

        self.assertEqual(source, repaired)
        connect.assert_called_once_with(source, target, {})

    def test_existing_smk25_route_is_left_untouched(self):
        selector = self.selector
        source = "Midi-Bridge:SINCO 3:(capture_1) SINCO SMK25-Master"
        target = selector.SMK25_MIDI_INPUT
        responses = {"-o": source + "\n", "-i": target + "\n"}

        def run(command, environment):
            return selector.subprocess.CompletedProcess(
                command, 0, responses[command[-1]], ""
            )

        snapshot = source + "\n  |-> " + target + "\n"
        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links", return_value=snapshot), \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_smk25_midi_route({})

        self.assertIsNone(repaired)
        connect.assert_not_called()

    def test_absent_device_or_router_port_is_not_treated_as_an_error(self):
        selector = self.selector
        commands = []

        def run(command, environment):
            commands.append(command)
            output = "" if command[-1] == "-o" else "SMK25 Pad Layers:midi-in\n"
            return selector.subprocess.CompletedProcess(command, 0, output, "")

        with patch.object(selector, "run", side_effect=run), \
                patch.object(selector, "links") as links, \
                patch.object(selector, "connect") as connect:
            repaired = selector.ensure_smk25_midi_route({})

        self.assertIsNone(repaired)
        self.assertEqual([["pw-link", "-o"]], commands)
        links.assert_not_called()
        connect.assert_not_called()


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
