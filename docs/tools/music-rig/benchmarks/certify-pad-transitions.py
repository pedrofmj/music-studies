#!/usr/bin/env python3
"""Certify Arturia pad transitions against the live user-session graph."""

from __future__ import annotations

import argparse
import json
import os
import random
import re
import subprocess
import time
from pathlib import Path


FULL_SERVICE = "music-rig-pedro-carla-headless.service"
SELECTOR_SERVICE = "music-rig-arturia-profile-session.service"
GENRES = {
    4: "worship-piano",
    5: "gospel-keys",
    6: "ambient-worship",
    7: "jazz-keys",
    8: "soul-rnb",
    9: "cinematic-strings",
    10: "orchestral",
    11: "brass-winds",
    12: "synthwave",
    13: "retro-keys",
    14: "intimate-pads",
    15: "praise-leads",
    16: "acoustic-worship",
}
CARLA_PATTERN = r"^/usr/bin/python3 /app/share/carla/carla "


def run(command: list[str]) -> str:
    result = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=3.0,
    )
    return result.stdout


def active(unit: str) -> bool:
    return subprocess.run(
        ["systemctl", "--user", "is-active", "--quiet", unit],
        check=False,
        timeout=3.0,
    ).returncode == 0


def send_pad(fifo: Path, pad: int) -> None:
    descriptor = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
    try:
        os.write(descriptor, bytes((pad,)))
    finally:
        os.close(descriptor)


def process_present(pattern: str) -> bool:
    result = subprocess.run(
        ["pgrep", "-f", pattern], check=False, stdout=subprocess.DEVNULL, timeout=3.0
    )
    return result.returncode == 0


def graph_links() -> str:
    return run(["pw-link", "-l"])


def audio_ready(links: str) -> bool:
    status = run(["wpctl", "status", "-n"])
    return (
        "auto_null" not in status.split("Settings", 1)[0]
        and "alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__sink" in status
        and "LSP Mixer x8 Stereo:Output L" in links
        and "SMC-MIX - 8-Band EQ:Input L" in links
    )


def check_pad(pad: int) -> tuple[bool, str]:
    if not active(SELECTOR_SERVICE):
        return False, "selector-inactive"
    links = graph_links()
    common = (
        "LSP Mixer x8 Stereo:Input L" in links
        and "SMC-MIX - 8-Band EQ:Output L" in links
        and "Arturia Main Volume Encoder:audio-out-l" in links
    )
    if not common or not audio_ready(links):
        return False, "master-output-links-missing"
    if pad == 1:
        ok = active(FULL_SERVICE) and process_present(
            r"^/home/ldap/pedro\.ferreira/.local/share/music-rig/arturia-profile-session/setBfree "
        )
        return ok, "organ" if ok else "organ-not-ready"
    if pad == 2:
        ok = active(FULL_SERVICE) and process_present(
            r"synthv1_jack --no-gui --client-name s2-synthv1-live"
        )
        return ok, "synth" if ok else "synth-not-ready"
    if pad == 3:
        ok = active(FULL_SERVICE) and "AR-CH-1 - Basic Piano:output_1" in links
        return ok, "full-live" if ok else "full-live-not-ready"
    project = GENRES[pad]
    project_pattern = rf"{re.escape(project)}\.uproject$"
    ok = (
        not active(FULL_SERVICE)
        and process_present(CARLA_PATTERN + ".*genre-projects/" + project_pattern)
        and "GENRE-CH-1" in links
        and links.count("GENRE-CH-") >= 20
    )
    return ok, project if ok else f"{project}-not-ready"


def wait_for_pad(fifo: Path, pad: int, timeout: float) -> dict[str, object]:
    send_pad(fifo, pad)
    deadline = time.monotonic() + timeout
    reason = "timeout"
    while time.monotonic() < deadline:
        ok, reason = check_pad(pad)
        if ok:
            return {"pad": pad, "status": "pass", "detail": reason}
        time.sleep(1.0)
    return {"pad": pad, "status": "fail", "detail": reason}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--control-fifo", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=100.0)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--sequence", type=int, nargs="*")
    parser.add_argument("--no-restore", action="store_true")
    arguments = parser.parse_args()

    sequence = arguments.sequence
    if not sequence:
        generator = random.Random(arguments.seed)
        sequence = generator.sample(range(1, 17), 16)
    if sorted(sequence) != list(range(1, 17)):
        parser.error("sequence must contain each pad from 1 through 16 exactly once")

    results = [wait_for_pad(arguments.control_fifo, pad, arguments.timeout) for pad in sequence]
    if not arguments.no_restore:
        results.append(wait_for_pad(arguments.control_fifo, 3, arguments.timeout))
    output = {"sequence": sequence, "results": results}
    print(json.dumps(output, indent=2, sort_keys=True))
    return 0 if all(item["status"] == "pass" for item in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
