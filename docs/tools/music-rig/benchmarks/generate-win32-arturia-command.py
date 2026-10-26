#!/usr/bin/env python3
"""Generate the explicit Windows Carla command from the shared SF2 catalog."""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from arturia_shared_sf2_catalog import build_profiles

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_PATCH_CATALOG = SCRIPT_DIR / "arturia-genre-patches.json"
DEFAULT_ROUTING = SCRIPT_DIR / "arturia-shared-sf2-routing.json"


def load_json(path):
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def csv(values):
    return ",".join(str(value) for value in values)


def generate_command(arguments, plugin_names, profiles):
    by_id = {profile_id: (indexes, layers)
             for profile_id, _, indexes, layers in profiles}
    initial = by_id.get(arguments.initial_profile)
    if initial is None:
        raise ValueError("unknown initial profile: " + arguments.initial_profile)

    indexes, layers = initial
    command = [
        "music-rigd", "run-win32-carla",
        "--carla-dll", arguments.carla_dll,
        "--project", arguments.project,
        "--driver", arguments.driver,
        "--client", arguments.client,
        "--midi-client", arguments.midi_client,
        "--midi-port", arguments.midi_port,
        "--input-interface", arguments.input_interface,
        "--loopback-output-interface", arguments.loopback_output_interface,
        "--patch-names", csv(plugin_names),
        "--patch-indexes", csv(indexes),
        "--active-layers", csv(layers),
        "--master-volume", str(arguments.master_volume),
    ]
    for _, pad_code, pad_indexes, pad_layers in profiles:
        command.extend(("--pad-profile", "%u:%s:%s" % (
            pad_code, csv(pad_indexes), csv(pad_layers)
        )))
    return subprocess.list2cmdline(command)


def write_patch_manifest(path, plugin_names, plugin_paths, profiles):
    manifest = {
        "schema": "music-studies/win32-carla-sf2-patches/v1",
        "patches": [
            {"instance_name": name, "sf2_path": sf2_path}
            for name, sf2_path in zip(plugin_names, plugin_paths)
        ],
        "pad_profiles": [
            {
                "profile_id": profile_id,
                "management_pad_code": pad_code,
                "patch_indexes": indexes,
                "active_layers": layers,
            }
            for profile_id, pad_code, indexes, layers in profiles
        ],
    }
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def self_test():
    patch_catalog = load_json(DEFAULT_PATCH_CATALOG)
    routing = load_json(DEFAULT_ROUTING)
    plugin_names, plugin_paths, profiles = build_profiles(patch_catalog, routing)

    if len(profiles) != 15 or profiles[0][1] != 1 or profiles[1][1] != 3 or \
            [profile[1] for profile in profiles[2:]] != list(range(4, 17)):
        raise ValueError("Arturia management-pad profile mapping changed")
    if len(plugin_names) >= 30 or len(set(plugin_names)) != len(plugin_names):
        raise ValueError("shared-SF2 Carla plugin instance list is invalid")
    if len(set(plugin_paths)) != 15:
        raise ValueError("unique shared-SF2 asset count changed")
    for profile_id, _, indexes, layers in profiles:
        active_indexes = [indexes[layer - 1] for layer in layers]
        if len(set(active_indexes)) != len(active_indexes):
            raise ValueError("profile reuses an SF2 plugin in active layers: " +
                             profile_id)
    command_args = argparse.Namespace(
        initial_profile="worship-piano",
        carla_dll="carla.dll",
        project="rig.carxp",
        driver="Windows Audio",
        client="music-rig",
        midi_client="Basic Loopback",
        midi_port="MIDI In",
        input_interface="physical-input",
        loopback_output_interface="loopback-output",
        master_volume=127,
    )
    command = generate_command(command_args, plugin_names, profiles)
    if command.count("--pad-profile") != len(profiles) or \
            "--patch-names" not in command:
        raise ValueError("generated command omitted the patch or pad catalog")
    with tempfile.TemporaryDirectory() as temporary_directory:
        manifest_path = Path(temporary_directory) / "patch-manifest.json"
        write_patch_manifest(
            manifest_path, plugin_names, plugin_paths, profiles
        )
        manifest = load_json(manifest_path)
        if len(manifest["patches"]) != len(plugin_names) or \
                len(manifest["pad_profiles"]) != len(profiles) or \
                manifest["patches"][0]["instance_name"] != plugin_names[0]:
            raise ValueError("generated Carla patch manifest is inconsistent")
    print("Windows Carla profile command: PASS "
          "(profiles=%d, plugin_instances=%d, unique_sf2=%d)" %
          (len(profiles), len(plugin_names), len(set(plugin_paths))))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--catalog", type=Path, default=DEFAULT_PATCH_CATALOG)
    parser.add_argument("--routing", type=Path, default=DEFAULT_ROUTING)
    parser.add_argument("--initial-profile", default="worship-piano")
    parser.add_argument(
        "--patch-manifest",
        type=Path,
        help="write Carla plugin instance names, SF2 paths, and pad plans as JSON",
    )
    parser.add_argument("--carla-dll")
    parser.add_argument("--project")
    parser.add_argument("--driver")
    parser.add_argument("--client")
    parser.add_argument("--midi-client")
    parser.add_argument("--midi-port")
    parser.add_argument("--input-interface")
    parser.add_argument("--loopback-output-interface")
    parser.add_argument("--master-volume", type=int, default=127)
    arguments = parser.parse_args(argv)

    try:
        if arguments.self_test:
            self_test()
            return 0
        required = ("carla_dll", "project", "driver", "client", "midi_client",
                    "midi_port", "input_interface", "loopback_output_interface")
        missing = [name.replace("_", "-") for name in required
                   if not getattr(arguments, name)]
        if missing:
            parser.error("required for command generation: " + ", ".join(missing))
        if not 0 <= arguments.master_volume <= 127:
            raise ValueError("master volume must be in 0..127")
        plugin_names, plugin_paths, profiles = build_profiles(
            load_json(arguments.catalog), load_json(arguments.routing)
        )
        if arguments.patch_manifest is not None:
            write_patch_manifest(
                arguments.patch_manifest, plugin_names, plugin_paths, profiles
            )
        print(generate_command(arguments, plugin_names, profiles))
    except (OSError, json.JSONDecodeError, ValueError) as error:
        print("Windows Carla profile command failed: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
