#!/usr/bin/env python3
"""Verify cold-start ordering and late USB MIDI alias discovery."""

from __future__ import annotations

from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader
import sys
import time
import unittest
from pathlib import Path
from unittest.mock import patch


def load_profile_cli(path: Path):
    loader = SourceFileLoader("music_rig_profile_cli", str(path))
    spec = spec_from_loader(loader.name, loader)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load profile CLI: {path}")
    module = module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class RecoveryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cli = load_profile_cli(Path(sys.argv.pop(1)))

    def test_recovery_starts_services_before_waiting_for_rack_ports(self):
        cli = self.cli
        started = []
        expected = [cli.HEADLESS, cli.ENCODER, cli.SMK, cli.SELECTOR]

        def service_state(unit):
            return "active" if unit in started else "inactive"

        def start_unit(command):
            unit = command[-1]
            started.append(unit)
            return cli.subprocess.CompletedProcess(command, 0, "", "")

        def wait_for_live_rack():
            self.assertEqual(expected, started)

        with patch.object(cli, "arturia_keylab_output", return_value="keylab"), \
                patch.object(cli, "service_state", side_effect=service_state), \
                patch.object(cli, "run", side_effect=start_unit), \
                patch.object(cli, "wait_for_live_rack", side_effect=wait_for_live_rack), \
                patch.object(cli, "ensure_arturia_router", return_value={"selector": "active"}), \
                patch.object(cli, "repair_audio_output", return_value={"status": "audio-repaired"}), \
                patch.object(cli, "repair_routes", return_value={"status": "routes-repaired"}):
            result = cli.recover_live_rig()

        self.assertEqual(expected, started)
        self.assertEqual("rig-recovered", result["status"])

    def test_recovery_does_not_start_or_restart_healthy_services(self):
        cli = self.cli
        with patch.object(cli, "arturia_keylab_output", return_value="keylab"), \
                patch.object(cli, "service_state", return_value="active"), \
                patch.object(cli, "run") as run, \
                patch.object(cli, "wait_for_live_rack"), \
                patch.object(cli, "ensure_arturia_router", return_value={"selector": "active"}), \
                patch.object(cli, "repair_audio_output", return_value={"status": "audio-repaired"}), \
                patch.object(cli, "repair_routes", return_value={"status": "routes-repaired"}):
            result = cli.recover_live_rig()

        run.assert_not_called()
        self.assertEqual("rig-recovered", result["status"])

    def test_start_retries_a_transient_destructive_systemd_transaction(self):
        cli = self.cli
        command = ["systemctl", "--user", "start", cli.HEADLESS]
        conflict = cli.subprocess.CompletedProcess(
            command, 1, "", "Transaction is destructive: a stop job is queued"
        )
        started = cli.subprocess.CompletedProcess(command, 0, "", "")
        with patch.object(cli, "run", side_effect=(conflict, started)) as run, \
                patch.object(cli.time, "sleep") as sleep:
            cli.start_service(cli.HEADLESS)

        self.assertEqual(2, run.call_count)
        sleep.assert_called_once_with(1)

    def test_device_alias_discovery_retries_late_usb_ports(self):
        cli = self.cli
        aliases = list(cli.DEVICE_MIDI_TARGETS)
        source_ports = "\n".join(
            f"Midi-Bridge:SINCO:{alias}:(capture_1) {alias}" for alias in aliases
        )
        targets = {
            target for alias_targets in cli.DEVICE_MIDI_TARGETS.values()
            for target in alias_targets
        }
        output_sequence = iter(("", source_ports))

        with patch.object(cli, "pipewire_output_ports", side_effect=lambda: next(output_sequence)), \
                patch.object(cli, "pipewire_input_ports", return_value="\n".join(targets)), \
                patch.object(cli, "sleep_for_route_retry"):
            sources, missing_sources, missing_targets = cli.wait_for_device_midi_ports(
                time.monotonic() + 5
            )

        self.assertEqual(set(aliases), set(sources))
        self.assertTrue(all(sources.values()))
        self.assertEqual([], missing_sources)
        self.assertEqual([], missing_targets)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
