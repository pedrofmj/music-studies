#!/usr/bin/env python3
"""Materialize one Arturia genre Carla project from real library patches."""

from __future__ import annotations

import argparse
import base64
import copy
import json
import textwrap
import xml.etree.ElementTree as ET
from pathlib import Path


AR_PREFIX = "AR-CH-"
CHUNK_PREFIX = b"VC2!\x10\r\x00\x00"


def excluded_name(name: str) -> bool:
    return (
        name.startswith(AR_PREFIX)
        or name.startswith("AR Controls")
        or name.startswith("Arturia Main Volume Encoder")
    )


def load(path: Path):
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def require_file(path: Path, label: str) -> Path:
    if not path.is_file():
        raise ValueError(f"{label} does not exist: {path}")
    return path


def preset_chunk(path: Path) -> str:
    root = ET.parse(path).getroot()
    xml = ET.tostring(root, encoding="utf-8", xml_declaration=True)
    encoded = base64.b64encode(CHUNK_PREFIX + xml + b"\x00").decode("ascii")
    return "\n".join(textwrap.wrap(encoded, 100))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-project", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--genre", required=True)
    parser.add_argument("--soundfont-root", type=Path, required=True)
    parser.add_argument("--decent-sampler-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        source = require_file(arguments.source_project, "source Carla project")
        manifest = load(arguments.manifest)
        patches = manifest.get("patches", {})
        selected = manifest.get("genres", {}).get(arguments.genre)
        if not isinstance(selected, list) or len(selected) != 9:
            raise ValueError(f"genre {arguments.genre!r} must contain exactly nine patches")

        project = ET.parse(source)
        root = project.getroot()
        engine_settings = root.find("EngineSettings")
        if engine_settings is not None:
            engine_settings.find("PreferUiBridges").text = "false"
            engine_settings.find("UIBridgesTimeout").text = "1000"
        for plugin in root.findall("Plugin"):
            name = plugin.findtext("Info/Name") or ""
            if name == "PD-CH-1 Output Gain 0 to +12 dB":
                for parameter in plugin.findall("Data/Parameter"):
                    if parameter.findtext("Symbol") == "g_out":
                        parameter.find("Value").text = "4"
            if name != "LSP Mixer x8 Stereo":
                continue
            for parameter in plugin.findall("Data/Parameter"):
                if parameter.findtext("Symbol") == "g_out":
                    parameter.find("Value").text = "1"
        arturia_plugins = [
            plugin for plugin in root.findall("Plugin")
            if (plugin.findtext("Info/Name") or "").startswith(AR_PREFIX)
            and "Output Trim" not in (plugin.findtext("Info/Name") or "")
            and "Volume Map" not in (plugin.findtext("Info/Name") or "")
            and "Reverb Map" not in (plugin.findtext("Info/Name") or "")
        ]
        map_plugins = {
            plugin.findtext("Info/Name"): plugin
            for plugin in root.findall("Plugin")
            if (plugin.findtext("Info/Name") or "").startswith("AR-CH-")
            and ("Volume Map" in (plugin.findtext("Info/Name") or "")
                 or "Reverb Map" in (plugin.findtext("Info/Name") or ""))
        }
        if len(arturia_plugins) != 9:
            raise ValueError(f"expected nine Arturia instrument plugins, found {len(arturia_plugins)}")
        vst2_template = next(
            (plugin for plugin in arturia_plugins if plugin.findtext("Info/Type") == "VST2"),
            None,
        )
        sf2_template = next(
            (plugin for plugin in arturia_plugins if plugin.findtext("Info/Type") == "SF2"),
            None,
        )
        if vst2_template is None or sf2_template is None:
            raise ValueError("source project must contain both VST2 and SF2 Arturia templates")

        retained_maps = []
        for channel in (2, 4, 5, 6, 7, 8, 9):
            for kind in ("Volume Map", "Reverb Map"):
                source_name = f"AR-CH-{channel} {kind}"
                source = map_plugins.get(source_name)
                if source is None:
                    raise ValueError(f"required map plugin is missing: {source_name}")
                retained = copy.deepcopy(source)
                retained.find("Info/Name").text = f"GENRE-CH-{channel} {kind}"
                retained_maps.append(retained)

        for child in list(root):
            if (child.tag == "Plugin" and child not in arturia_plugins
                    and excluded_name(child.findtext("Info/Name") or "")):
                root.remove(child)
            elif child.tag == "ExternalPatchbay":
                for connection in list(child):
                    source_name = connection.findtext("Source") or ""
                    target_name = connection.findtext("Target") or ""
                    if excluded_name(source_name) or excluded_name(target_name):
                        child.remove(connection)

        for retained in retained_maps:
            root.append(retained)

        for index, (plugin, patch_id) in enumerate(zip(arturia_plugins, selected), 1):
            patch = patches.get(patch_id)
            if not isinstance(patch, dict):
                raise ValueError(f"unknown patch {patch_id!r}")
            expected_type = "VST2" if patch["engine"] == "decent-sampler" else "SF2"
            if plugin.findtext("Info/Type") != expected_type:
                replacement = copy.deepcopy(vst2_template if expected_type == "VST2" else sf2_template)
                position = list(root).index(plugin)
                root.remove(plugin)
                root.insert(position, replacement)
                plugin = replacement

            info = plugin.find("Info")
            data = plugin.find("Data")
            if info is None or data is None:
                raise ValueError(f"Arturia plugin {index} is incomplete")
            name = f"GENRE-CH-{index} - {patch['label']}"
            original_name = info.find("Name")
            if original_name is None:
                raise ValueError(f"Arturia plugin {index} has no name")
            original_name.text = name
            if patch["engine"] == "sf2":
                filename = info.find("Filename")
                label = info.find("Label")
                if filename is None or label is None:
                    raise ValueError(f"SF2 plugin {index} has no filename/label")
                asset = require_file(arguments.soundfont_root / patch["path"], patch_id)
                filename.text = str(asset)
                label.text = patch["label"]
            elif patch["engine"] == "decent-sampler":
                asset = require_file(arguments.decent_sampler_root / patch["path"], patch_id)
                chunk = data.find("Chunk")
                if chunk is None:
                    raise ValueError(f"DecentSampler plugin {index} has no state chunk")
                chunk.text = "\n" + preset_chunk(asset) + "\n"
            else:
                raise ValueError(f"unsupported patch engine {patch['engine']!r}")

        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        project.write(arguments.output, encoding="utf-8", xml_declaration=True)
        print(json.dumps({
            "genre": arguments.genre,
            "output": str(arguments.output),
            "plugins": len(arturia_plugins),
            "patches": selected,
            "schema": "music-studies/arturia-genre-project/v1",
            "status": "materialized-candidate-project"
        }, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, ET.ParseError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
