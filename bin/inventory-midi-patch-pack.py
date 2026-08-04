#!/usr/bin/env python3
"""Create a read-only, program-level inventory of a MIDI patch package.

The script deliberately does not modify the source directory.  SoundFont
sample payloads are not decoded: only RIFF headers, metadata, and the pdta
program/instrument/sample tables are read.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import mimetypes
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from typing import Any, Iterator


SCHEMA_VERSION = "https://music-studies.local/schemas/midi-patch-library-inventory/v1"

INFO_CHUNKS = {
    b"ifil": "soundfont_version",
    b"isng": "target_sound_engine",
    b"INAM": "name",
    b"irom": "rom_name",
    b"iver": "rom_version",
    b"ICRD": "creation_date",
    b"IENG": "engineer",
    b"IPRD": "product",
    b"ICOP": "copyright",
    b"ICMT": "comments",
    b"ISFT": "software",
}

GENERATOR_NAMES = {
    0: "start_address_offset",
    1: "end_address_offset",
    2: "start_loop_address_offset",
    3: "end_loop_address_offset",
    4: "start_address_coarse_offset",
    5: "modulation_lfo_to_pitch",
    6: "vibrato_lfo_to_pitch",
    7: "modulation_envelope_to_pitch",
    8: "initial_filter_cutoff",
    9: "initial_filter_resonance",
    10: "modulation_lfo_to_filter_cutoff",
    11: "modulation_envelope_to_filter_cutoff",
    12: "end_address_coarse_offset",
    13: "modulation_lfo_to_volume",
    15: "chorus_effects_send",
    16: "reverb_effects_send",
    17: "pan",
    21: "delay_modulation_lfo",
    22: "frequency_modulation_lfo",
    23: "delay_vibrato_lfo",
    24: "frequency_vibrato_lfo",
    25: "delay_modulation_envelope",
    26: "attack_modulation_envelope",
    27: "hold_modulation_envelope",
    28: "decay_modulation_envelope",
    29: "sustain_modulation_envelope",
    30: "release_modulation_envelope",
    31: "key_number_to_modulation_envelope_hold",
    32: "key_number_to_modulation_envelope_decay",
    33: "delay_volume_envelope",
    34: "attack_volume_envelope",
    35: "hold_volume_envelope",
    36: "decay_volume_envelope",
    37: "sustain_volume_envelope",
    38: "release_volume_envelope",
    39: "key_number_to_volume_envelope_hold",
    40: "key_number_to_volume_envelope_decay",
    41: "instrument",
    43: "key_range",
    44: "velocity_range",
    45: "start_loop_address_coarse_offset",
    46: "key_number",
    47: "velocity",
    48: "initial_attenuation",
    49: "end_loop_address_coarse_offset",
    50: "end_loop_address_coarse_offset",
    51: "coarse_tune",
    52: "fine_tune",
    53: "sample_id",
    54: "sample_modes",
    56: "scale_tuning",
    57: "exclusive_class",
}

SAMPLE_TYPES = {
    1: "mono",
    2: "right",
    4: "left",
    8: "linked",
}

THEME_PATTERNS = (
    ("star wars", "Star Wars", "film"),
    ("harry potter", "Harry Potter", "film"),
    ("lord of the rings", "The Lord of the Rings", "film"),
    ("jurassic park", "Jurassic Park", "film"),
    ("pirates of the caribbean", "Pirates of the Caribbean", "film"),
    ("ghostbusters", "Ghostbusters", "film"),
    ("james bond", "James Bond", "film"),
    ("star trek", "Star Trek", "film and television"),
    ("stranger things", "Stranger Things", "television"),
    ("mario", "Super Mario", "video game"),
    ("zelda", "The Legend of Zelda", "video game"),
    ("sonic", "Sonic the Hedgehog", "video game"),
    ("pokemon", "Pokemon", "video game"),
    ("final fantasy", "Final Fantasy", "video game"),
    ("chrono trigger", "Chrono Trigger", "video game"),
    ("castlevania", "Castlevania", "video game"),
    ("megaman", "Mega Man", "video game"),
    ("mega man", "Mega Man", "video game"),
    ("minecraft", "Minecraft", "video game"),
    ("undertale", "Undertale", "video game"),
    ("street fighter", "Street Fighter", "video game"),
    ("mortal kombat", "Mortal Kombat", "video game"),
    ("dragon ball", "Dragon Ball", "anime"),
    ("naruto", "Naruto", "anime"),
)

CATEGORY_KEYWORDS = (
    ("accordion", ("accordion", "sanfona")),
    ("brass", ("brass", "trumpet", "trompeta", "trombone", "sax", "metal")),
    ("bass", ("bass", "baixo")),
    ("drums_and_percussion", ("drum", "kit", "perc", "beat", "kick", "snare")),
    ("guitar", ("guitar", "violao", "violão", "guitarra")),
    ("keyboard", ("keyboard", "keys", "key " )),
    ("organ", ("organ", "hammond")),
    ("piano", ("piano", "rhodes", "wurlitzer")),
    ("synth", ("synth", "lead", "arp", "pulse", "pluck")),
    ("pad", ("pad", "ambient")),
    ("strings", ("string", "violin", "cello", "orchestra")),
    ("voice_and_choir", ("voice", "choir", "vocal")),
)


def decode_text(value: bytes) -> str:
    """Decode RIFF text, retaining non-ASCII metadata if it is present."""
    value = value.split(b"\0", 1)[0].rstrip(b" \t\r\n")
    if not value:
        return ""
    try:
        return value.decode("utf-8")
    except UnicodeDecodeError:
        return value.decode("latin-1", errors="replace")


def signed_16(value: int) -> int:
    return value - 65536 if value >= 32768 else value


def utc_timestamp(timestamp: float) -> str:
    return dt.datetime.fromtimestamp(timestamp, dt.timezone.utc).isoformat().replace("+00:00", "Z")


def classify_name(value: str) -> list[str]:
    lowered = f" {value.lower()} "
    return [category for category, words in CATEGORY_KEYWORDS if any(word in lowered for word in words)]


def theme_candidates(*values: str) -> list[dict[str, str]]:
    text = " ".join(value.lower() for value in values if value)
    matches = []
    for needle, title, medium in THEME_PATTERNS:
        if needle in text:
            matches.append(
                {
                    "title": title,
                    "medium": medium,
                    "confidence": "name_match_only",
                    "evidence": needle,
                    "review_status": "unverified",
                }
            )
    return matches


class SoundFontError(ValueError):
    pass


class SoundFontFile:
    """Read the metadata and pdta tables of an SF2 or SF3 RIFF file."""

    def __init__(self, path: Path):
        self.path = path
        self.file_size = path.stat().st_size
        self.handle = path.open("rb")
        header = self.handle.read(12)
        if len(header) != 12 or header[:4] != b"RIFF" or header[8:12] != b"sfbk":
            raise SoundFontError("not a RIFF SoundFont (expected RIFF ... sfbk)")
        riff_size = struct.unpack("<I", header[4:8])[0]
        self.riff_end = min(self.file_size, riff_size + 8)
        if self.riff_end < 12:
            raise SoundFontError("invalid RIFF length")

    def close(self) -> None:
        self.handle.close()

    def chunks(self, start: int, end: int) -> Iterator[tuple[bytes, int, int]]:
        position = start
        while position + 8 <= end:
            self.handle.seek(position)
            header = self.handle.read(8)
            if len(header) != 8:
                raise SoundFontError("truncated chunk header")
            chunk_id, size = struct.unpack("<4sI", header)
            data_start = position + 8
            data_end = data_start + size
            if data_end > end:
                raise SoundFontError(f"truncated {chunk_id.decode('latin-1')} chunk")
            yield chunk_id, data_start, size
            position = data_end + (size % 2)

    def read(self, start: int, size: int) -> bytes:
        self.handle.seek(start)
        data = self.handle.read(size)
        if len(data) != size:
            raise SoundFontError("truncated chunk data")
        return data

    def list_chunks(self, start: int, size: int) -> tuple[bytes, list[tuple[bytes, int, int]]]:
        if size < 4:
            raise SoundFontError("LIST chunk is smaller than its list type")
        list_type = self.read(start, 4)
        return list_type, list(self.chunks(start + 4, start + size))


def fixed_records(data: bytes, record_size: int, name: str) -> list[bytes]:
    if len(data) % record_size:
        raise SoundFontError(f"{name} length {len(data)} is not a multiple of {record_size}")
    return [data[index:index + record_size] for index in range(0, len(data), record_size)]


def generator_record(data: bytes, references: list[dict[str, Any]] | None = None) -> dict[str, Any]:
    operator, amount = struct.unpack("<HH", data)
    result: dict[str, Any] = {
        "operator": operator,
        "property": GENERATOR_NAMES.get(operator, f"reserved_{operator}"),
        "value_unsigned": amount,
        "value_signed": signed_16(amount),
    }
    if operator in (43, 44):
        result["range"] = {"low": amount & 0xFF, "high": amount >> 8}
    elif operator in (41, 53):
        result["index"] = amount
        if references is not None and amount < len(references):
            result["reference_name"] = references[amount]["name"]
        elif references is not None:
            result["reference_error"] = "index outside available table"
    return result


def modulator_record(data: bytes) -> dict[str, Any]:
    source, destination, amount, amount_source, transform = struct.unpack("<HHhHH", data)
    return {
        "source_operator": source,
        "destination_operator": destination,
        "destination_property": GENERATOR_NAMES.get(destination, f"reserved_{destination}"),
        "amount": amount,
        "amount_source_operator": amount_source,
        "transform": transform,
    }


def zones_for(
    bags: list[tuple[int, int]],
    generators: list[bytes],
    modulators: list[bytes],
    first_bag: int,
    next_bag: int,
    references: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    if first_bag > next_bag or next_bag >= len(bags):
        raise SoundFontError("invalid bag index range")
    zones = []
    for bag_index in range(first_bag, next_bag):
        first_generator, first_modulator = bags[bag_index]
        next_generator, next_modulator = bags[bag_index + 1]
        if first_generator > next_generator or next_generator > len(generators):
            raise SoundFontError("invalid generator index range")
        if first_modulator > next_modulator or next_modulator > len(modulators):
            raise SoundFontError("invalid modulator index range")
        zone_generators = [
            generator_record(record, references) for record in generators[first_generator:next_generator]
        ]
        zones.append(
            {
                "bag_index": bag_index,
                "is_global_zone": not any(item["operator"] in (41, 53) for item in zone_generators),
                "generators": zone_generators,
                "modulators": [modulator_record(record) for record in modulators[first_modulator:next_modulator]],
            }
        )
    return zones


def parse_info(data: bytes) -> dict[str, Any]:
    result: dict[str, Any] = {}
    position = 0
    while position + 8 <= len(data):
        chunk_id, size = struct.unpack("<4sI", data[position:position + 8])
        value_start = position + 8
        value_end = value_start + size
        if value_end > len(data):
            raise SoundFontError("truncated INFO subchunk")
        key = INFO_CHUNKS.get(chunk_id, f"unknown_{chunk_id.decode('latin-1')}")
        value = data[value_start:value_end]
        if chunk_id in (b"ifil", b"iver") and len(value) >= 4:
            major, minor = struct.unpack("<HH", value[:4])
            result[key] = {"major": major, "minor": minor}
        else:
            result[key] = decode_text(value)
        position = value_end + (size % 2)
    return result


def parse_soundfont(path: Path) -> dict[str, Any]:
    soundfont = SoundFontFile(path)
    try:
        info_data: bytes | None = None
        pdta_chunks: dict[bytes, bytes] = {}
        chunk_summary: list[dict[str, Any]] = []
        sample_encoding = "unidentified"

        for chunk_id, start, size in soundfont.chunks(12, soundfont.riff_end):
            chunk_name = chunk_id.decode("latin-1")
            if chunk_id != b"LIST":
                chunk_summary.append({"id": chunk_name, "bytes": size})
                continue
            list_type, children = soundfont.list_chunks(start, size)
            list_name = list_type.decode("latin-1")
            child_summary = [
                {"id": child_id.decode("latin-1"), "bytes": child_size}
                for child_id, _child_start, child_size in children
            ]
            chunk_summary.append({"id": "LIST", "type": list_name, "bytes": size, "children": child_summary})
            if list_type == b"INFO":
                info_data = soundfont.read(start + 4, size - 4)
            elif list_type == b"pdta":
                for child_id, child_start, child_size in children:
                    pdta_chunks[child_id] = soundfont.read(child_start, child_size)
            elif list_type == b"sdta":
                for child_id, child_start, _child_size in children:
                    if child_id == b"smpl":
                        sample_encoding = "ogg_vorbis" if soundfont.read(child_start, min(4, _child_size)) == b"OggS" else "pcm_16_bit"

        if info_data is None:
            raise SoundFontError("INFO list is missing")
        info = parse_info(info_data)
        required = (b"phdr", b"pbag", b"pmod", b"pgen", b"inst", b"ibag", b"imod", b"igen", b"shdr")
        missing = [chunk.decode("latin-1") for chunk in required if chunk not in pdta_chunks]
        if missing:
            raise SoundFontError("pdta list is missing " + ", ".join(missing))

        phdr = fixed_records(pdta_chunks[b"phdr"], 38, "phdr")
        pbag = [struct.unpack("<HH", record) for record in fixed_records(pdta_chunks[b"pbag"], 4, "pbag")]
        pmod = fixed_records(pdta_chunks[b"pmod"], 10, "pmod")
        pgen = fixed_records(pdta_chunks[b"pgen"], 4, "pgen")
        inst = fixed_records(pdta_chunks[b"inst"], 22, "inst")
        ibag = [struct.unpack("<HH", record) for record in fixed_records(pdta_chunks[b"ibag"], 4, "ibag")]
        imod = fixed_records(pdta_chunks[b"imod"], 10, "imod")
        igen = fixed_records(pdta_chunks[b"igen"], 4, "igen")
        shdr = fixed_records(pdta_chunks[b"shdr"], 46, "shdr")
        if not phdr or not inst or not shdr or not pbag or not ibag:
            raise SoundFontError("a required pdta table is empty")

        samples = []
        for record in shdr[:-1]:
            name = decode_text(record[:20])
            start, end, loop_start, loop_end, sample_rate, original_pitch, pitch_correction, link, sample_type = struct.unpack(
                "<IIIIIBbHH", record[20:]
            )
            samples.append(
                {
                    "name": name,
                    "start": start,
                    "end": end,
                    "loop_start": loop_start,
                    "loop_end": loop_end,
                    "sample_rate_hz": sample_rate,
                    "original_pitch_midi_note": original_pitch,
                    "pitch_correction_cents": pitch_correction,
                    "linked_sample_index": link,
                    "sample_type": sample_type,
                    "sample_type_name": SAMPLE_TYPES.get(sample_type & 0x7FFF, "unknown"),
                    "is_rom_sample": bool(sample_type & 0x8000),
                }
            )

        instruments = []
        for index, record in enumerate(inst[:-1]):
            name = decode_text(record[:20])
            first_bag = struct.unpack("<H", record[20:])[0]
            next_bag = struct.unpack("<H", inst[index + 1][20:])[0]
            instruments.append(
                {
                    "index": index,
                    "name": name,
                    "classification": classify_name(name),
                    "theme_candidates": theme_candidates(name),
                    "zones": zones_for(ibag, igen, imod, first_bag, next_bag, samples),
                }
            )

        presets = []
        for index, record in enumerate(phdr[:-1]):
            name = decode_text(record[:20])
            preset, bank, first_bag, library, genre, morphology = struct.unpack("<HHHIII", record[20:])
            next_bag = struct.unpack("<H", phdr[index + 1][24:26])[0]
            presets.append(
                {
                    "index": index,
                    "name": name,
                    "bank": bank,
                    "program": preset,
                    "program_address": f"{bank}:{preset}",
                    "bank_type": "percussion" if bank == 128 else "melodic",
                    "library": library,
                    "genre": genre,
                    "morphology": morphology,
                    "classification": classify_name(name),
                    "theme_candidates": theme_candidates(name),
                    "zones": zones_for(pbag, pgen, pmod, first_bag, next_bag, instruments),
                }
            )

        return {
            "format": "SF3" if path.suffix.lower() == ".sf3" or sample_encoding == "ogg_vorbis" else "SF2",
            "riff_form": "sfbk",
            "sample_encoding": sample_encoding,
            "embedded_metadata": info,
            "embedded_author": info.get("engineer") or None,
            "chunk_layout": chunk_summary,
            "table_counts": {
                "presets": len(presets),
                "instruments": len(instruments),
                "samples": len(samples),
                "preset_bags_including_terminal": len(pbag),
                "preset_generators": len(pgen),
                "preset_modulators": len(pmod),
                "instrument_bags_including_terminal": len(ibag),
                "instrument_generators": len(igen),
                "instrument_modulators": len(imod),
            },
            "presets": presets,
            "instruments": instruments,
            "samples": samples,
        }
    finally:
        soundfont.close()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def pdf_properties(path: Path) -> dict[str, Any]:
    result: dict[str, Any] = {"inspection": "pdfinfo" if shutil.which("pdfinfo") else "unavailable"}
    if not shutil.which("pdfinfo"):
        return result
    try:
        completed = subprocess.run(
            ["pdfinfo", str(path)], text=True, capture_output=True, timeout=15, check=False
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        result["inspection_error"] = str(error)
        return result
    if completed.returncode:
        result["inspection_error"] = completed.stderr.strip() or f"pdfinfo exited {completed.returncode}"
        return result
    for line in completed.stdout.splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            result[key.strip().lower().replace(" ", "_")] = value.strip()
    return result


def read_bencode(data: bytes, position: int = 0) -> tuple[Any, int]:
    marker = data[position:position + 1]
    if marker == b"i":
        end = data.index(b"e", position)
        return int(data[position + 1:end]), end + 1
    if marker == b"l":
        result = []
        position += 1
        while data[position:position + 1] != b"e":
            value, position = read_bencode(data, position)
            result.append(value)
        return result, position + 1
    if marker == b"d":
        result: dict[bytes, Any] = {}
        position += 1
        while data[position:position + 1] != b"e":
            key, position = read_bencode(data, position)
            value, position = read_bencode(data, position)
            result[key] = value
        return result, position + 1
    colon = data.index(b":", position)
    length = int(data[position:colon])
    start = colon + 1
    return data[start:start + length], start + length


def torrent_properties(path: Path) -> dict[str, Any]:
    try:
        decoded, _position = read_bencode(path.read_bytes())
        if not isinstance(decoded, dict):
            raise ValueError("top-level torrent value is not a dictionary")
        info = decoded.get(b"info", {})
        convert = lambda value: decode_text(value) if isinstance(value, bytes) else value
        result = {
            "announce": convert(decoded.get(b"announce")),
            "name": convert(info.get(b"name")) if isinstance(info, dict) else None,
            "piece_length": info.get(b"piece length") if isinstance(info, dict) else None,
            "private": info.get(b"private") if isinstance(info, dict) else None,
        }
        return {key: value for key, value in result.items() if value is not None}
    except (IndexError, KeyError, ValueError, UnicodeDecodeError) as error:
        return {"inspection_error": str(error)}


def generic_file_type(path: Path) -> str:
    suffix = path.suffix.lower()
    return {
        ".json": "audio_evolution_preset",
        ".pdf": "supporting_document",
        ".rar": "archive",
        ".torrent": "torrent_metadata",
    }.get(suffix, mimetypes.guess_type(path.name)[0] or "unclassified_file")


def inspect_file(path: Path, source: Path, include_sha256: bool) -> dict[str, Any]:
    stat = path.stat()
    relative_path = path.relative_to(source).as_posix()
    record: dict[str, Any] = {
        "relative_path": relative_path,
        "source_pack": relative_path.split("/", 1)[0],
        "filename": path.name,
        "extension": path.suffix.lower(),
        "bytes": stat.st_size,
        "modified_at": utc_timestamp(stat.st_mtime),
        "classification": classify_name(f"{relative_path} {path.stem}"),
        "theme_candidates": theme_candidates(relative_path, path.stem),
        "organization": {"phase": 1, "status": "unplanned", "proposed_path": None},
    }
    if include_sha256:
        record["sha256"] = sha256(path)
    else:
        record["sha256"] = None
        record["sha256_status"] = "not_calculated; use --sha256 to scan complete sample payloads"

    try:
        if path.suffix.lower() in (".sf2", ".sf3"):
            record["type"] = "soundfont_bank"
            record["soundfont"] = parse_soundfont(path)
            record["embedded_author"] = record["soundfont"]["embedded_author"]
            record["classification"] = sorted(
                set(record["classification"] + classify_name(record["soundfont"]["embedded_metadata"].get("name", "")))
            )
        elif path.suffix.lower() == ".json":
            record["type"] = "audio_evolution_preset"
            record["preset"] = json.loads(path.read_text(encoding="utf-8-sig"))
            record["embedded_author"] = None
        elif path.suffix.lower() == ".pdf":
            record["type"] = "supporting_document"
            record["pdf"] = pdf_properties(path)
            record["embedded_author"] = record["pdf"].get("author")
        elif path.suffix.lower() == ".torrent":
            record["type"] = "torrent_metadata"
            record["torrent"] = torrent_properties(path)
            record["embedded_author"] = None
        else:
            record["type"] = generic_file_type(path)
            record["embedded_author"] = None
    except (OSError, SoundFontError, UnicodeDecodeError, json.JSONDecodeError, struct.error, ValueError) as error:
        record["inspection_error"] = str(error)
    return record


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Extracted package root; its top-level zip directory is excluded")
    parser.add_argument("output", type=Path, help="Inventory JSON to create or replace")
    parser.add_argument(
        "--sha256",
        action="store_true",
        help="Calculate SHA-256 for every file, including large SoundFont sample payloads",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    source = args.source.resolve()
    output = args.output.resolve()
    if not source.is_dir():
        print(f"Source directory does not exist: {source}", file=sys.stderr)
        return 2
    if output.is_relative_to(source):
        print("Output must be outside the source package directory", file=sys.stderr)
        return 2

    files = []
    skipped = []
    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        if relative.parts and relative.parts[0].lower() == "zip":
            skipped.append(relative.as_posix())
            continue
        files.append(inspect_file(path, source, args.sha256))

    by_type: dict[str, int] = {}
    total_bytes = 0
    errors = 0
    for record in files:
        by_type[record["type"]] = by_type.get(record["type"], 0) + 1
        total_bytes += record["bytes"]
        errors += int("inspection_error" in record)
    inventory = {
        "schema": SCHEMA_VERSION,
        "generated_at": utc_timestamp(dt.datetime.now(tz=dt.timezone.utc).timestamp()),
        "phase": 1,
        "source": {
            "path": str(source),
            "scope": "extracted package directories only",
            "excluded_top_level_directories": ["zip"],
            "source_is_modified": False,
        },
        "organization": {
            "phase": 1,
            "status": "not_planned",
            "proposed_structure": None,
            "application_status": "not_started",
        },
        "summary": {
            "file_count": len(files),
            "total_bytes": total_bytes,
            "by_type": dict(sorted(by_type.items())),
            "inspection_error_count": errors,
            "excluded_zip_file_count": len(skipped),
            "sha256_complete": args.sha256,
        },
        "files": files,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=output.parent, delete=False) as temporary:
        json.dump(inventory, temporary, ensure_ascii=False, indent=2)
        temporary.write("\n")
        temporary_path = Path(temporary.name)
    temporary_path.replace(output)
    print(json.dumps(inventory["summary"], ensure_ascii=False, sort_keys=True))
    print(f"Wrote {output}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
