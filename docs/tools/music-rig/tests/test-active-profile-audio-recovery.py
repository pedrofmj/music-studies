#!/usr/bin/env python3
"""Check recovery of active profile audio links without PipeWire hardware."""

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


class ActiveProfileAudioRecoveryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.selector = load_selector(sys.argv.pop(1))

    def test_fast_engine_profile_audio_routes_target_live_mixer(self):
        selector = self.selector

        self.assertEqual(
            (
                ("setBfree:out_left", "LSP Mixer x8 Stereo:Input L"),
                ("setBfree:out_right", "LSP Mixer x8 Stereo:Input R"),
            ),
            selector.fast_engine_audio_connections("tonewheel-organ-setbfree"),
        )
        self.assertEqual(
            (
                ("s2-synthv1-live:out_1", "LSP Mixer x8 Stereo:Input L"),
                ("s2-synthv1-live:out_2", "LSP Mixer x8 Stereo:Input R"),
            ),
            selector.fast_engine_audio_connections("synth-programmer-synthv1"),
        )

    def test_restores_missing_profile_audio_links(self):
        selector = self.selector
        routes = selector.fast_engine_audio_connections("tonewheel-organ-setbfree")

        with patch.object(selector, "links", return_value=""), \
                patch.object(selector, "connect") as connect:
            restored = selector.ensure_audio_routes(routes, {})

        self.assertEqual(list(routes), restored)
        self.assertEqual(2, connect.call_count)
        connect.assert_any_call(routes[0][0], routes[0][1], {})
        connect.assert_any_call(routes[1][0], routes[1][1], {})

    def test_existing_profile_audio_links_are_left_untouched(self):
        selector = self.selector
        routes = selector.fast_engine_audio_connections("tonewheel-organ-setbfree")
        graph = (
            "setBfree:out_left\n"
            "  |-> Carla.188/LSP Mixer x8 Stereo:Input L\n"
            "setBfree:out_right\n"
            "  |-> Carla.188/LSP Mixer x8 Stereo:Input R\n"
        )

        with patch.object(selector, "links", return_value=graph), \
                patch.object(selector, "connect") as connect:
            restored = selector.ensure_audio_routes(routes, {})

        self.assertEqual([], restored)
        connect.assert_not_called()


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
