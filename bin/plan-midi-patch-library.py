#!/usr/bin/env python3
"""Add a phase-2 organization proposal to a patch-library inventory JSON.

This script changes only the inventory document. It never reads, moves, or
renames files in the source package.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
from collections import defaultdict
from pathlib import Path
import re
import tempfile
from typing import Any


LIBRARY_ROOT = "Library"

TREE = [
    "Library/01_Audio_Evolution/01_Piano_and_Keys",
    "Library/01_Audio_Evolution/02_Pads_and_Ambience",
    "Library/01_Audio_Evolution/03_Synth_Leads_Arps_and_Bass",
    "Library/01_Audio_Evolution/04_Strings_Brass_and_Voices",
    "Library/01_Audio_Evolution/99_Review",
    "Library/02_SoundFonts/01_Acoustic_Pianos",
    "Library/02_SoundFonts/02_Electric_Pianos_and_Clavs",
    "Library/02_SoundFonts/03_Organs_and_Keys",
    "Library/02_SoundFonts/04_Accordions",
    "Library/02_SoundFonts/05_Strings_and_Orchestral",
    "Library/02_SoundFonts/06_Brass_and_Winds",
    "Library/02_SoundFonts/07_Guitars",
    "Library/02_SoundFonts/08_Basses",
    "Library/02_SoundFonts/09_Drums_and_Percussion",
    "Library/02_SoundFonts/10_Synth_Leads_Arps_and_Plucks",
    "Library/02_SoundFonts/11_Pads_and_Ambience",
    "Library/02_SoundFonts/12_Vocals_and_Choirs",
    "Library/02_SoundFonts/13_Layered_and_Multi_Instrument",
    "Library/02_SoundFonts/99_Review/Unclassified",
    "Library/90_Reference/Documents",
    "Library/90_Reference/Torrent_Metadata",
    "Library/91_Archives/Included_Archives",
    "Library/99_Review/Unreadable_or_Truncated",
]


def utc_now() -> str:
    return dt.datetime.now(tz=dt.timezone.utc).isoformat().replace("+00:00", "Z")


def normalized_text(record: dict[str, Any]) -> str:
    soundfont = record.get("soundfont") or {}
    metadata = soundfont.get("embedded_metadata") or {}
    path_parts = Path(record.get("relative_path", "")).parts
    # The source-pack name is a collection label, not evidence about every file
    # within it. For example, one pack name mentions synths, brass, and accordions.
    source_relative_path = "/".join(path_parts[1:]) if len(path_parts) > 1 else record.get("filename", "")
    names = [source_relative_path, metadata.get("name", "")]
    names.extend(preset.get("name", "") for preset in soundfont.get("presets", []))
    return " ".join(names).casefold()


def all_tags(record: dict[str, Any]) -> set[str]:
    soundfont = record.get("soundfont") or {}
    tags: set[str] = set()
    for preset in soundfont.get("presets", []):
        tags.update(preset.get("classification", []))
    return tags


def has_any(text: str, values: tuple[str, ...]) -> bool:
    return any(value in text for value in values)


def soundfont_destination(record: dict[str, Any]) -> tuple[str, str, str]:
    """Return target directory, category, and a concise evidence string."""
    if record.get("inspection_error"):
        return (
            "99_Review/Unreadable_or_Truncated",
            "unreadable_or_truncated",
            "The SoundFont parser reported an inspection error.",
        )

    text = normalized_text(record)
    tags = all_tags(record)
    base = "02_SoundFonts"
    if "drums_and_percussion" in tags or has_any(text, ("drum", "percussion", "gamelan", "gamelin")):
        return f"{base}/09_Drums_and_Percussion", "drums_and_percussion", "Name or embedded preset data indicates drums or percussion."
    if "accordion" in tags or has_any(text, ("accordion", "acordeon", "acordeão", "sanfona")):
        return f"{base}/04_Accordions", "accordion", "Name or embedded preset data indicates accordion."
    if "bass" in tags or has_any(text, (" bass", "baixo", "contrabaixo")):
        return f"{base}/08_Basses", "bass", "Name or embedded preset data indicates bass."
    if "guitar" in tags or has_any(text, ("guitar", "guitarra", "violao", "violão")):
        return f"{base}/07_Guitars", "guitar", "Name or embedded preset data indicates guitar."
    if "brass" in tags or has_any(text, ("brass", "trumpet", "tromp", "sax", "metais", "metaleira", "horn")):
        return f"{base}/06_Brass_and_Winds", "brass_and_winds", "Name or embedded preset data indicates brass or wind instruments."
    if "strings" in tags or has_any(text, ("strings", "string", "orchestra", "orchestral", "violin", "cello")):
        return f"{base}/05_Strings_and_Orchestral", "strings_and_orchestral", "Name or embedded preset data indicates strings or orchestral instruments."
    if "voice_and_choir" in tags or has_any(text, ("voice", "vox", "choir", "vocal")):
        return f"{base}/12_Vocals_and_Choirs", "vocals_and_choirs", "Name or embedded preset data indicates voice or choir."
    if has_any(text, ("piano e pad", "piano & pad", "piano pad", "layer", " mix pack", "stack", "full motif")):
        return f"{base}/13_Layered_and_Multi_Instrument", "layered_and_multi_instrument", "Name indicates a layered or multi-instrument bank."
    if "organ" in tags or has_any(text, ("organ", "hammond")):
        return f"{base}/03_Organs_and_Keys", "organs_and_keys", "Name or embedded preset data indicates organ or keyboard."
    if has_any(text, ("rhodes", "wurlitzer", "electric piano", " ep", "ep5", "ep6", "tines", " clav", "clav ")):
        return f"{base}/02_Electric_Pianos_and_Clavs", "electric_pianos_and_clavs", "Name indicates an electric piano or clavinet sound."
    if "piano" in tags or has_any(text, ("piano", "grand", "upright", "uphight", " cfx", "silver grand", "royal grand", "white grand", "studio grand", "alicia")):
        return f"{base}/01_Acoustic_Pianos", "acoustic_pianos", "Name or embedded preset data indicates an acoustic piano."
    if "pad" in tags or has_any(text, ("pad", "ambient", "ambience", "shimmer", "atmos", "newage", "new age")):
        return f"{base}/11_Pads_and_Ambience", "pads_and_ambience", "Name or embedded preset data indicates a pad or ambient sound."
    if "synth" in tags or has_any(text, ("synth", "lead", "arp", "pluck", "moog", "prophet", "dx7", "d-50", "saw", "trance")):
        return f"{base}/10_Synth_Leads_Arps_and_Plucks", "synth_leads_arps_and_plucks", "Name or embedded preset data indicates a synth sound."
    if "keyboard" in tags or has_any(text, ("nord stage", "motif", "keyboard", "keys", "bells")):
        return f"{base}/03_Organs_and_Keys", "organs_and_keys", "Name indicates a keyboard collection or unclassified key sound."
    return f"{base}/99_Review/Unclassified", "unclassified", "No reliable family classification was found in the name or embedded preset data."


def audio_evolution_destination(record: dict[str, Any]) -> tuple[str, str, str]:
    text = normalized_text(record)
    tags = all_tags(record)
    base = "01_Audio_Evolution"
    if "piano" in tags or "keyboard" in tags or has_any(text, ("piano", "keys", "keyboard")):
        return f"{base}/01_Piano_and_Keys", "piano_and_keys", "Name indicates a piano or keyboard preset."
    if "pad" in tags or has_any(text, ("pad", "ambient", "shimmer")):
        return f"{base}/02_Pads_and_Ambience", "pads_and_ambience", "Name indicates a pad or ambient preset."
    if "strings" in tags or "brass" in tags or "voice_and_choir" in tags:
        return f"{base}/04_Strings_Brass_and_Voices", "acoustic_ensemble", "Name indicates strings, brass, or voice."
    if "synth" in tags or "bass" in tags or has_any(text, ("synth", "lead", "arp", "bass", "pulse")):
        return f"{base}/03_Synth_Leads_Arps_and_Bass", "synth_leads_arps_and_bass", "Name indicates a synth, arpeggio, or bass preset."
    return f"{base}/99_Review", "unclassified", "No reliable preset family classification was found."


def destination_for(record: dict[str, Any]) -> tuple[str, str, str, str]:
    file_type = record["type"]
    if file_type == "soundfont_bank":
        directory, category, rationale = soundfont_destination(record)
        status = "needs_review" if category in {"unreadable_or_truncated", "unclassified"} else "proposed"
    elif file_type == "audio_evolution_preset":
        directory, category, rationale = audio_evolution_destination(record)
        status = "needs_review" if category == "unclassified" else "proposed"
    elif file_type == "supporting_document":
        directory, category, rationale, status = "90_Reference/Documents", "document", "Supporting PDF document.", "proposed"
    elif file_type == "torrent_metadata":
        directory, category, rationale, status = "90_Reference/Torrent_Metadata", "torrent_metadata", "Download metadata, not a playable patch.", "proposed"
    else:
        directory, category, rationale, status = "91_Archives/Included_Archives", "archive", "Included archive, not an extracted playable patch.", "proposed"
    return directory, category, rationale, status


def source_slug(relative_path: str) -> str:
    source_pack = relative_path.split("/", 1)[0]
    slug = re.sub(r"[^a-z0-9]+", "-", source_pack.casefold()).strip("-")
    return slug[:40] or "source"


def apply_collision_policy(records: list[dict[str, Any]]) -> int:
    by_target: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for record in records:
        by_target[record["organization"]["proposed_path"]].append(record)
    collisions = 0
    for target, entries in by_target.items():
        if len(entries) == 1:
            continue
        collisions += len(entries)
        for ordinal, record in enumerate(sorted(entries, key=lambda item: item["relative_path"]), start=1):
            path = Path(target)
            suffix = f" [{source_slug(record['relative_path'])}-{ordinal}]"
            filename = f"{path.stem}{suffix}{path.suffix}"
            record["organization"]["proposed_path"] = str(path.with_name(filename))
            record["organization"]["filename_collision"] = True
    return collisions


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path, help="Phase-1 inventory JSON")
    parser.add_argument("output", type=Path, help="JSON to create or replace; may equal inventory")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    inventory_path = args.inventory.resolve()
    output_path = args.output.resolve()
    with inventory_path.open(encoding="utf-8") as handle:
        inventory: dict[str, Any] = json.load(handle)
    if inventory.get("phase") not in (1, 2):
        raise ValueError("Expected a phase-1 or phase-2 inventory JSON")

    for record in inventory["files"]:
        directory, category, rationale, status = destination_for(record)
        record["organization"] = {
            "phase": 2,
            "status": status,
            "current_relative_path": record["relative_path"],
            "proposed_category": category,
            "proposed_path": f"{LIBRARY_ROOT}/{directory}/{record['filename']}",
            "rationale": rationale,
            "filename_collision": False,
            "application_status": "not_applied",
        }
    collision_count = apply_collision_policy(inventory["files"])
    status_counts: dict[str, int] = defaultdict(int)
    category_counts: dict[str, int] = defaultdict(int)
    for record in inventory["files"]:
        organization = record["organization"]
        status_counts[organization["status"]] += 1
        category_counts[organization["proposed_category"]] += 1

    inventory["phase"] = 2
    inventory["organization"] = {
        "phase": 2,
        "status": "proposed",
        "approval_status": "pending_user_review",
        "proposed_root": LIBRARY_ROOT,
        "proposed_tree": TREE,
        "rules": [
            "Keep Audio Evolution JSON presets separate from SF2 and SF3 banks.",
            "Organize playable banks by primary sound family, using embedded preset names when available.",
            "Keep supporting documents, torrent metadata, and included archives outside playable-patch folders.",
            "Route unreadable or ambiguous entries to review folders; do not discard them.",
            "Keep every source file during application; do not deduplicate until SHA-256 values are reviewed.",
        ],
        "collision_policy": {
            "status": "planned",
            "method": "Append source-pack slug and ordinal only where proposed target paths collide.",
            "affected_file_count": collision_count,
        },
        "deduplication": {
            "status": "deferred",
            "required_before_application": "Run the phase-1 inventory with --sha256 and review exact duplicate groups.",
            "action": "No file will be removed, merged, or replaced during phase 3 without explicit approval.",
        },
        "assignment_summary": {
            "by_status": dict(sorted(status_counts.items())),
            "by_category": dict(sorted(category_counts.items())),
        },
        "application_status": "not_started",
        "planned_at": utc_now(),
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=output_path.parent, delete=False) as temporary:
        json.dump(inventory, temporary, ensure_ascii=False, indent=2)
        temporary.write("\n")
        temporary_path = Path(temporary.name)
    temporary_path.replace(output_path)
    print(json.dumps(inventory["organization"]["assignment_summary"], indent=2, sort_keys=True))
    print(f"Wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
