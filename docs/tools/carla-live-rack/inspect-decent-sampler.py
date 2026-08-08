#!/usr/bin/env python3
"""List DecentSampler VST2 parameters after loading a Carla project chunk."""

from __future__ import annotations

import argparse
import base64
import ctypes
import xml.etree.ElementTree as ET
from pathlib import Path


INTPTR = ctypes.c_ssize_t


class AEffect(ctypes.Structure):
    pass


AEffectPointer = ctypes.POINTER(AEffect)
AudioMasterCallback = ctypes.CFUNCTYPE(
    INTPTR,
    AEffectPointer,
    ctypes.c_int32,
    ctypes.c_int32,
    INTPTR,
    ctypes.c_void_p,
    ctypes.c_float,
)
Dispatcher = ctypes.CFUNCTYPE(
    INTPTR,
    AEffectPointer,
    ctypes.c_int32,
    ctypes.c_int32,
    INTPTR,
    ctypes.c_void_p,
    ctypes.c_float,
)
Process = ctypes.CFUNCTYPE(
    None,
    AEffectPointer,
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.POINTER(ctypes.POINTER(ctypes.c_float)),
    ctypes.c_int32,
)
SetParameter = ctypes.CFUNCTYPE(None, AEffectPointer, ctypes.c_int32, ctypes.c_float)
GetParameter = ctypes.CFUNCTYPE(ctypes.c_float, AEffectPointer, ctypes.c_int32)

AEffect._fields_ = [
    ("magic", ctypes.c_int32),
    ("dispatcher", Dispatcher),
    ("process", Process),
    ("set_parameter", SetParameter),
    ("get_parameter", GetParameter),
    ("num_programs", ctypes.c_int32),
    ("num_parameters", ctypes.c_int32),
    ("num_inputs", ctypes.c_int32),
    ("num_outputs", ctypes.c_int32),
    ("flags", ctypes.c_int32),
    ("reserved_1", INTPTR),
    ("reserved_2", INTPTR),
    ("initial_delay", ctypes.c_int32),
    ("real_qualities", ctypes.c_int32),
    ("off_qualities", ctypes.c_int32),
    ("io_ratio", ctypes.c_float),
    ("object", ctypes.c_void_p),
    ("user", ctypes.c_void_p),
    ("unique_id", ctypes.c_int32),
    ("version", ctypes.c_int32),
    ("process_replacing", Process),
    ("process_double_replacing", ctypes.c_void_p),
    ("future", ctypes.c_char * 56),
]


def project_chunk(project: Path, plugin_name: str) -> bytes:
    root = ET.parse(project).getroot()
    for plugin in root.findall("Plugin"):
        if plugin.findtext("Info/Name") != plugin_name:
            continue
        encoded = "".join((plugin.findtext("Data/Chunk") or "").split())
        data = base64.b64decode(encoded)
        if data[:4] == b"VC2!":
            return data[8:]
        return data
    raise SystemExit(f"plugin not found: {plugin_name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path)
    parser.add_argument("plugin_name")
    parser.add_argument(
        "--binary",
        type=Path,
        default=Path.home() / ".vst/DecentSampler.so",
    )
    args = parser.parse_args()

    @AudioMasterCallback
    def host_callback(_effect, opcode, _index, _value, _pointer, _opt):
        return 2400 if opcode == 1 else 0

    library = ctypes.CDLL(str(args.binary))
    entry = library.VSTPluginMain
    entry.argtypes = [AudioMasterCallback]
    entry.restype = AEffectPointer
    effect = entry(host_callback)
    if not effect or effect.contents.magic != 0x56737450:
        raise SystemExit("DecentSampler did not return a valid VST2 AEffect")

    effect.contents.dispatcher(effect, 0, 0, 0, None, 0.0)
    try:
        effect.contents.dispatcher(effect, 10, 0, 0, None, 48000.0)
        effect.contents.dispatcher(effect, 11, 0, 512, None, 0.0)
        chunk = project_chunk(args.project, args.plugin_name)
        chunk_buffer = ctypes.create_string_buffer(chunk)
        result = effect.contents.dispatcher(
            effect,
            24,
            0,
            len(chunk),
            ctypes.cast(chunk_buffer, ctypes.c_void_p),
            0.0,
        )
        print(f"set_chunk={result} parameters={effect.contents.num_parameters}")
        for index in range(effect.contents.num_parameters):
            name = ctypes.create_string_buffer(256)
            effect.contents.dispatcher(effect, 8, index, 0, name, 0.0)
            value = effect.contents.get_parameter(effect, index)
            print(f"{index}\t{name.value.decode(errors='replace')}\t{value:.9f}")
    finally:
        effect.contents.dispatcher(effect, 1, 0, 0, None, 0.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
