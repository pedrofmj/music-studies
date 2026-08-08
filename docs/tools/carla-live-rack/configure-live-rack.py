#!/usr/bin/env python3
"""Build Pedro's reproducible multi-controller Carla live rack."""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import os
import shutil
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


SOUND_ROOT = Path(
    "/home/ldap/pedro.ferreira/Flash/PED/MIDI/Pack de Timbres/Library"
)

ASSETS = {
    "nord": (
        SOUND_ROOT / "02_SoundFonts/01_Acoustic_Pianos/Nord White Grand Full 24C.sf2",
        "Nord White Grand Full 24C",
    ),
    "flute": (
        Path("/home/ldap/pedro.ferreira/.local/share/sounds/sf2/FluidR3_GM.sf2"),
        "Flute",
    ),
    "sax": (
        SOUND_ROOT / "02_SoundFonts/06_Brass_and_Winds/SAX_Lirakeys LITE.sf2",
        "SAX Lirakeys CL",
    ),
    "hammond": (
        SOUND_ROOT
        / "02_SoundFonts/03_Organs_and_Keys"
        / "Hammond_B3_organ_Fast_Leslie [outros-timbres-pra-audio-evolution-leads-2].sf2",
        "Hammond_B3_organ_Fast_Leslie [outros-timbres-pra-audio-evolution-leads-2]",
    ),
    "optik": (
        SOUND_ROOT / "02_SoundFonts/10_Synth_Leads_Arps_and_Plucks/G072 Optik_Synth.sf2",
        "G072 Optik_Synth",
    ),
    "pad_fx": (
        SOUND_ROOT / "02_SoundFonts/11_Pads_and_Ambience/PAD EFEITOS 1 _ Infinity_studio.sf2",
        "PAD EFEITOS 1 _ Infinity_studio",
    ),
    "atmosphere": (
        SOUND_ROOT / "02_SoundFonts/11_Pads_and_Ambience/AtmosferaPAD - TIMBRES PREMIUM.sf2",
        "AtmosferaPAD - TIMBRES PREMIUM",
    ),
    "drums": (
        SOUND_ROOT / "02_SoundFonts/09_Drums_and_Percussion/Drum_Set.sf2",
        "Drum_Set",
    ),
    "oceans": (
        SOUND_ROOT / "02_SoundFonts/11_Pads_and_Ambience/OceansPad - TIMBRES PREMIUM.sf2",
        "OceansPad - TIMBRES",
    ),
}

ARTURIA_SOURCE = "Midi-Bridge:KL Essential 61 mk3 1:(capture_0) KL Essential 61 mk3 MIDI"
PAD_POCKET_SOURCE = "Midi-Bridge:SINCO 2:(capture_1) SINCO SMC-PAD Pocket-Master"
PAD_SOURCE = (
    "Midi-Bridge:Jieli Technology SINCO at usb-0000:00:14-0-4-2- full speed:"
    "(capture_1) SINCO SMC-PAD-Master"
)
SMK_SOURCE = (
    "Midi-Bridge:Jieli Technology SINCO at usb-0000:00:14-0-4-3- full speed:"
    "(capture_1) SINCO SMK25-Master"
)
SMC_MIXER_SOURCE = (
    "Midi-Bridge:Jieli Technology SINCO at usb-0000:00:14-0-4-4- full speed:"
    "(capture_1) SINCO SMC-Mixer-Master"
)

ARTURIA_SCALE = "AR Controls - Sustain Scale"
PAD_SCALE = "PD Controls - Sustain Scale"
MASTER_MIXER = "LSP Mixer x8 Stereo"
MASTER_EQ = "SMC-MIX - 8-Band EQ"


def plugin_name(plugin: ET.Element) -> str:
    return plugin.findtext("Info/Name") or ""


def find_plugin(root: ET.Element, *names: str) -> ET.Element:
    for plugin in root.findall("Plugin"):
        if plugin_name(plugin) in names:
            return plugin
    raise ValueError(f"required source plugin not found: {', '.join(names)}")


def find_plugin_by_uri(root: ET.Element, uri: str) -> ET.Element:
    for plugin in root.findall("Plugin"):
        if plugin.findtext("Info/URI") == uri:
            return plugin
    raise ValueError(f"required source plugin URI not found: {uri}")


def set_text(parent: ET.Element, path: str, value: object) -> ET.Element:
    element = parent.find(path)
    if element is None:
        element = ET.SubElement(parent, path)
    element.text = str(value)
    return element


def parameter(plugin: ET.Element, index: int, name: str, symbol: str | None = None) -> ET.Element:
    data = plugin.find("Data")
    if data is None:
        raise ValueError(f"plugin has no Data block: {plugin_name(plugin)}")
    for item in data.findall("Parameter"):
        if item.findtext("Index") == str(index):
            set_text(item, "Name", name)
            if symbol is not None:
                set_text(item, "Symbol", symbol)
            return item
    item = ET.Element("Parameter")
    insertion_index = len(data)
    for child_index, child in enumerate(data):
        if child.tag in ("CustomData", "Chunk"):
            insertion_index = child_index
            break
    data.insert(insertion_index, item)
    set_text(item, "Index", index)
    set_text(item, "Name", name)
    if symbol is not None:
        set_text(item, "Symbol", symbol)
    return item


def clear_mapping(item: ET.Element) -> None:
    for tag in (
        "MidiChannel",
        "MappedControlIndex",
        "MappedMinimum",
        "MappedMaximum",
        "MidiCC",
    ):
        child = item.find(tag)
        if child is not None:
            item.remove(child)


def map_parameter(
    item: ET.Element,
    cc: int,
    minimum: float | None = None,
    maximum: float | None = None,
) -> None:
    clear_mapping(item)
    set_text(item, "MidiChannel", 1)
    set_text(item, "MappedControlIndex", cc)
    if minimum is not None:
        set_text(item, "MappedMinimum", minimum)
    if maximum is not None:
        set_text(item, "MappedMaximum", maximum)
    set_text(item, "MidiCC", cc)


def set_info(plugin: ET.Element, name: str) -> None:
    set_text(plugin, "Info/Name", name)


def configure_decent_sampler(
    plugin: ET.Element,
    name: str,
    volume_cc: int,
    reverb_index: int,
    reverb_name: str,
    reverb_cc: int,
) -> ET.Element:
    plugin = copy.deepcopy(plugin)
    set_info(plugin, name)
    set_text(plugin, "Data/Volume", 1)
    map_parameter(parameter(plugin, 0, "Main Volume"), volume_cc, 0, 1)
    clear_mapping(parameter(plugin, 1, "Main Tuning"))
    map_parameter(parameter(plugin, reverb_index, reverb_name), reverb_cc, 0, 1)
    return plugin


def configure_sf2(
    template: ET.Element,
    name: str,
    asset_key: str,
    reverb_cc: int,
) -> ET.Element:
    plugin = copy.deepcopy(template)
    filename, label = ASSETS[asset_key]
    set_text(plugin, "Info/Type", "SF2")
    set_info(plugin, name)
    set_text(plugin, "Info/Filename", filename)
    set_text(plugin, "Info/Label", label)
    set_text(plugin, "Data/Volume", 1)
    map_parameter(parameter(plugin, 3, "Reverb Level"), reverb_cc)
    set_text(parameter(plugin, 3, "Reverb Level"), "Value", 0.35)
    return plugin


def configure_mapcc(template: ET.Element, name: str, cc_in: int, cc_out: int) -> ET.Element:
    plugin = copy.deepcopy(template)
    set_info(plugin, name)
    set_text(parameter(plugin, 1, "Filter Channel", "channelf"), "Value", 0)
    set_text(parameter(plugin, 2, "CC Input", "ccin"), "Value", cc_in)
    set_text(parameter(plugin, 3, "CC Output", "ccout"), "Value", cc_out)
    return plugin


def configure_scale(template: ET.Element, name: str) -> ET.Element:
    plugin = copy.deepcopy(template)
    set_info(plugin, name)
    return plugin


def configure_master_mixer(template: ET.Element) -> ET.Element:
    plugin = copy.deepcopy(template)
    set_text(plugin, "Data/Volume", 1)
    set_text(parameter(plugin, 2, "Output balance", "bal"), "Value", 0)
    set_text(parameter(plugin, 16, "Channel gain 1", "cg_1"), "Value", 1)
    return plugin


def build_eq() -> ET.Element:
    xml = f"""
<Plugin>
 <Info>
  <Type>LV2</Type>
  <Name>{MASTER_EQ}</Name>
  <URI>http://lsp-plug.in/plugins/lv2/para_equalizer_x8_stereo</URI>
 </Info>
 <Data>
  <Active>Yes</Active>
  <Volume>1</Volume>
  <ControlChannel>1</ControlChannel>
  <Options>0x1</Options>
  <CustomData>
   <Type>http://kxstudio.sf.net/ns/carla/property</Type>
   <Key>CarlaSkinIsCompacted</Key>
   <Value>false</Value>
  </CustomData>
 </Data>
</Plugin>
"""
    plugin = ET.fromstring(xml)
    for index, name, symbol in (
        (11, "Input FFT graph enable Left", "ife_l"),
        (12, "Output FFT graph enable Left", "ofe_l"),
        (13, "Input FFT graph enable Right", "ife_r"),
        (14, "Output FFT graph enable Right", "ofe_r"),
    ):
        set_text(parameter(plugin, index, name, symbol), "Value", 0)
    filter_types = (5, 1, 1, 1, 1, 1, 1, 3)
    frequencies = (63, 125, 250, 500, 1000, 2000, 4000, 8000)
    for band in range(8):
        base = 21 + 11 * band
        set_text(parameter(plugin, base, f"Filter type {band}", f"ft_{band}"), "Value", filter_types[band])
        set_text(parameter(plugin, base + 5, f"Frequency {band}", f"f_{band}"), "Value", frequencies[band])
        gain = parameter(plugin, base + 7, f"Gain {band}", f"g_{band}")
        map_parameter(gain, 102 + band, 0.2511886432, 3.981071706)
        set_text(gain, "Value", 1)
    return plugin


def add_connection(patchbay: ET.Element, source: str, target: str) -> None:
    connection = ET.Element("Connection")
    set_text(connection, "Source", source)
    set_text(connection, "Target", target)
    patchbay.append(connection)


def replace_patchbay(root: ET.Element, plugins: list[ET.Element]) -> None:
    patchbay = root.find("ExternalPatchbay")
    if patchbay is None:
        patchbay = ET.SubElement(root, "ExternalPatchbay")

    preserved = []
    for connection in patchbay.findall("Connection"):
        source = connection.findtext("Source") or ""
        if "ALSA Playback [java]" in source:
            preserved.append(
                (source, connection.findtext("Target") or "")
            )
    for child in list(patchbay):
        patchbay.remove(child)
    for source, target in preserved:
        add_connection(patchbay, source, target)

    instruments = [
        ("AR-CH-1 - Basic Piano", "output_1", "output_2"),
        ("AR-CH-2 - Nord White Grand Full 24C", "out-left", "out-right"),
        ("AR-CH-3 - Alt Strings", "output_1", "output_2"),
        ("AR-CH-4 - Good Flute", "out-left", "out-right"),
        ("AR-CH-5 - SAX Lirakeys CL", "out-left", "out-right"),
        ("AR-CH-6 - Hammond Organ Fast", "out-left", "out-right"),
        ("AR-CH-7 - Optik Synth", "out-left", "out-right"),
        ("AR-CH-8 - PAD EFEITOS", "out-left", "out-right"),
        ("AR-CH-9 - AtmosferaPAD", "out-left", "out-right"),
        ("PD-CH-1 - Drum Set", "out-left", "out-right"),
        ("SMK-CH-1 - Oceans Worship Pad", "out-left", "out-right"),
    ]
    for name, left, right in instruments:
        add_connection(patchbay, f"{name}:{left}", f"{MASTER_MIXER}:Input L")
        add_connection(patchbay, f"{name}:{right}", f"{MASTER_MIXER}:Input R")

    add_connection(patchbay, f"{MASTER_MIXER}:Output L", f"{MASTER_EQ}:Input L")
    add_connection(patchbay, f"{MASTER_MIXER}:Output R", f"{MASTER_EQ}:Input R")

    add_connection(patchbay, ARTURIA_SOURCE, f"{ARTURIA_SCALE}:events-in")
    add_connection(patchbay, PAD_POCKET_SOURCE, f"{PAD_SCALE}:events-in")
    add_connection(patchbay, PAD_SOURCE, f"{PAD_SCALE}:events-in")
    add_connection(patchbay, SMK_SOURCE, "SMK-CH-1 Volume Map:events-in")

    add_connection(patchbay, f"{ARTURIA_SCALE}:events-out", "AR-CH-1 - Basic Piano:events-in")
    add_connection(patchbay, f"{ARTURIA_SCALE}:events-out", "AR-CH-3 - Alt Strings:events-in")
    for channel in (2, 4, 5, 6, 7, 8, 9):
        add_connection(
            patchbay,
            f"{ARTURIA_SCALE}:events-out",
            f"AR-CH-{channel} Volume Map:events-in",
        )
    for channel, target in (
        (2, "AR-CH-2 - Nord White Grand Full 24C"),
        (4, "AR-CH-4 - Good Flute"),
        (5, "AR-CH-5 - SAX Lirakeys CL"),
        (6, "AR-CH-6 - Hammond Organ Fast"),
        (7, "AR-CH-7 - Optik Synth"),
        (8, "AR-CH-8 - PAD EFEITOS"),
        (9, "AR-CH-9 - AtmosferaPAD"),
    ):
        add_connection(patchbay, f"AR-CH-{channel} Volume Map:events-out", f"{target}:events-in")

    add_connection(patchbay, f"{PAD_SCALE}:events-out", "PD-CH-1 Volume Map:events-in")
    add_connection(patchbay, "PD-CH-1 Volume Map:events-out", "PD-CH-1 - Drum Set:events-in")
    add_connection(patchbay, "SMK-CH-1 Volume Map:events-out", "SMK-CH-1 - Oceans Worship Pad:events-in")

    for band in range(8):
        mapper = f"SMC-EQ-{band + 1} CC Scale"
        add_connection(patchbay, SMC_MIXER_SOURCE, f"{mapper}:events-in")
        add_connection(patchbay, f"{mapper}:events-out", f"{MASTER_EQ}:events-in")

    positions = ET.SubElement(patchbay, "Positions")
    for index, plugin in enumerate(plugins):
        position = ET.SubElement(
            positions,
            "Position",
            {
                "x1": str(60 + (index % 4) * 360),
                "y1": str(70 + (index // 4) * 150),
                "pluginId": str(index),
            },
        )
        set_text(position, "Name", plugin_name(plugin))


def build_plugins(root: ET.Element) -> list[ET.Element]:
    basic = find_plugin(root, "CH 1 - DecentSampler. Basic Piano", "AR-CH-1 - Basic Piano")
    strings = find_plugin(root, "CH 2 - DecentSampler Alt Strings", "AR-CH-3 - Alt Strings")
    sf2_template = find_plugin(
        root,
        "CH 3 - Hammond Organ Fast",
        "AR-CH-6 - Hammond Organ Fast",
    )
    mixer = copy.deepcopy(find_plugin(root, MASTER_MIXER))
    scale_template = find_plugin_by_uri(root, "http://gareus.org/oss/lv2/midifilter#scalecc")
    mapcc_template = find_plugin_by_uri(root, "http://gareus.org/oss/lv2/midifilter#mapcc")

    plugins = [
        configure_decent_sampler(basic, "AR-CH-1 - Basic Piano", 73, 3, "Reverb", 74),
        configure_sf2(sf2_template, "AR-CH-2 - Nord White Grand Full 24C", "nord", 71),
        configure_decent_sampler(strings, "AR-CH-3 - Alt Strings", 79, 7, "Reverb Mix", 76),
        configure_sf2(sf2_template, "AR-CH-4 - Good Flute", "flute", 77),
        configure_sf2(sf2_template, "AR-CH-5 - SAX Lirakeys CL", "sax", 93),
        configure_sf2(sf2_template, "AR-CH-6 - Hammond Organ Fast", "hammond", 18),
        configure_sf2(sf2_template, "AR-CH-7 - Optik Synth", "optik", 19),
        configure_sf2(sf2_template, "AR-CH-8 - PAD EFEITOS", "pad_fx", 16),
        configure_sf2(sf2_template, "AR-CH-9 - AtmosferaPAD", "atmosphere", 17),
        configure_sf2(sf2_template, "PD-CH-1 - Drum Set", "drums", 91),
        configure_sf2(sf2_template, "SMK-CH-1 - Oceans Worship Pad", "oceans", 91),
        configure_master_mixer(mixer),
        build_eq(),
        configure_scale(scale_template, ARTURIA_SCALE),
        configure_scale(scale_template, PAD_SCALE),
    ]

    for channel, cc in ((2, 75), (4, 72), (5, 80), (6, 81), (7, 82), (8, 83), (9, 85)):
        plugins.append(configure_mapcc(mapcc_template, f"AR-CH-{channel} Volume Map", cc, 7))
    plugins.extend(
        (
            configure_mapcc(mapcc_template, "PD-CH-1 Volume Map", 36, 7),
            configure_mapcc(mapcc_template, "SMK-CH-1 Volume Map", 20, 7),
        )
    )
    for band in range(8):
        plugins.append(
            configure_mapcc(
                mapcc_template,
                f"SMC-EQ-{band + 1} CC Scale",
                40 + band,
                102 + band,
            )
        )
    return plugins


def validate_assets() -> None:
    missing = [str(filename) for filename, _label in ASSETS.values() if not filename.is_file()]
    if missing:
        raise ValueError("missing sound assets:\n" + "\n".join(missing))


def validate_result(root: ET.Element, plugins: list[ET.Element]) -> None:
    names = [plugin_name(plugin) for plugin in plugins]
    if len(plugins) != 32 or len(names) != len(set(names)):
        raise ValueError("expected 32 uniquely named plugins")
    expected = {f"AR-CH-{channel}" for channel in range(1, 10)}
    found = {name.split(" - ", 1)[0] for name in names if name.startswith("AR-CH-") and "Volume Map" not in name}
    if found != expected:
        raise ValueError(f"Arturia instrument set mismatch: {sorted(found)}")
    connections = root.findall("ExternalPatchbay/Connection")
    if len(connections) != 65:
        raise ValueError(f"expected 65 project connections, found {len(connections)}")


def write_project(project: Path, root: ET.Element, make_backup: bool) -> Path | None:
    original_mode = project.stat().st_mode & 0o7777
    ET.indent(root, space=" ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    content = "<?xml version='1.0' encoding='UTF-8'?>\n<!DOCTYPE CARLA-PROJECT>\n" + body + "\n"
    backup = None
    if make_backup:
        stamp = dt.datetime.now().astimezone().strftime("%Y%m%dT%H%M%S%z")
        backup = project.with_name(f"{project.name}.before-live-rack-{stamp}")
        shutil.copy2(project, backup)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=project.parent, delete=False) as handle:
        handle.write(content)
        temporary = Path(handle.name)
    os.chmod(temporary, original_mode)
    os.replace(temporary, project)
    return backup


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path, nargs="?", default=Path("/c/music/carla/pedro.uproject"))
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--no-backup", action="store_true")
    args = parser.parse_args()

    validate_assets()
    tree = ET.parse(args.project)
    root = tree.getroot()
    plugins = build_plugins(root)

    for plugin in list(root.findall("Plugin")):
        root.remove(plugin)
    patchbay = root.find("ExternalPatchbay")
    insertion_index = list(root).index(patchbay) if patchbay is not None else len(root)
    for offset, plugin in enumerate(plugins):
        root.insert(insertion_index + offset, plugin)
    replace_patchbay(root, plugins)
    validate_result(root, plugins)

    if args.check_only:
        print(f"OK: would write {len(plugins)} plugins and 65 project connections")
        return 0
    backup = write_project(args.project, root, not args.no_backup)
    print(f"Updated: {args.project}")
    if backup is not None:
        print(f"Backup: {backup}")
    print(f"Plugins: {len(plugins)}; project connections: 65")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
