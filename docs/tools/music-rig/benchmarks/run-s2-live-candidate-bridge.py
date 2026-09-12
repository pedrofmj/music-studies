#!/usr/bin/env python3
"""Run a reversible Arturia-to-synthv1 live PipeWire bridge."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


EXPECTED_PROFILE = "synth-programmer-synthv1"
EXPECTED_BINDING = "synthv1-candidate"
EXPECTED_MIDI = "Midi-Bridge:KL Essential 61 mk3 2:(capture_0) KL Essential 61 mk3 MIDI"
EXPECTED_AUDIO_LEFT = (
    "alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic."
    "HiFi__hw_sofhdadsp__sink:playback_FL"
)
EXPECTED_AUDIO_RIGHT = EXPECTED_AUDIO_LEFT[:-2] + "FR"


def digest(value: str) -> str:
    return hashlib.sha256(value.encode()).hexdigest()


def command(args: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        env=environment,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )


def links(environment: dict[str, str]) -> str:
    result = command(["pw-link", "-l"], environment)
    if result.returncode != 0:
        raise RuntimeError(result.stdout)
    return result.stdout


def port_exists(environment: dict[str, str], direction: str, name: str) -> bool:
    result = command(["pw-link", direction, name], environment)
    return result.returncode == 0 and name in result.stdout


def link_present(snapshot: str, source: str, target: str) -> bool:
    return source in snapshot and target in snapshot


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--binding", required=True)
    parser.add_argument("--acknowledge-live-routing", action="store_true")
    parser.add_argument("--pw-jack", type=Path, default=Path("/usr/bin/pw-jack"))
    parser.add_argument("--synthv1", type=Path, required=True)
    parser.add_argument("--preset", type=Path, required=True)
    parser.add_argument("--controls-config", type=Path, required=True)
    parser.add_argument("--duration-ms", type=int, default=5000)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    if not arguments.acknowledge_live_routing:
        parser.error("explicit live-routing acknowledgement is required")
    if arguments.profile != EXPECTED_PROFILE or arguments.binding != EXPECTED_BINDING:
        parser.error("only the approved synthv1 candidate binding is supported")
    if arguments.duration_ms <= 0:
        parser.error("duration must be positive")

    if not arguments.controls_config.is_file():
        parser.error("candidate synthv1 controls config is missing")

    config_temporary = tempfile.TemporaryDirectory(prefix="music-rig-synthv1-config-")
    config_home = Path(config_temporary.name) / "config"
    config_file = config_home / "rncbc.org" / "synthv1.conf"
    config_file.parent.mkdir(parents=True)
    shutil.copyfile(arguments.controls_config, config_file)

    environment = os.environ.copy()
    environment.setdefault("XDG_RUNTIME_DIR", "/run/user/50001")
    environment.setdefault(
        "DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/50001/bus"
    )
    environment["XDG_CONFIG_HOME"] = str(config_home)
    client = "s2-synthv1-live"
    candidate_input = f"{client}:in"
    candidate_left = f"{client}:out_1"
    candidate_right = f"{client}:out_2"
    temporary_links = [
        (EXPECTED_MIDI, candidate_input),
        (candidate_left, EXPECTED_AUDIO_LEFT),
        (candidate_right, EXPECTED_AUDIO_RIGHT),
    ]
    before = ""
    process: subprocess.Popen[str] | None = None
    connected: list[tuple[str, str]] = []
    status = "live-routing-transaction-fail"
    error: str | None = None
    try:
        if not arguments.synthv1.is_file() or not arguments.preset.is_file():
            raise RuntimeError("candidate executable or preset is missing")
        before = links(environment)
        process = subprocess.Popen(
            [
                str(arguments.pw_jack),
                str(arguments.synthv1),
                "--no-gui",
                "--client-name",
                client,
                str(arguments.preset),
            ],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            if (
                port_exists(environment, "-o", candidate_left)
                and port_exists(environment, "-o", candidate_right)
                and port_exists(environment, "-i", candidate_input)
            ):
                break
            if process.poll() is not None:
                raise RuntimeError("candidate process exited before port registration")
            time.sleep(0.1)
        else:
            raise RuntimeError("candidate ports did not register")

        for source, target in temporary_links:
            result = command(["pw-link", source, target], environment)
            if result.returncode != 0:
                raise RuntimeError(f"link failed: {source} -> {target}: {result.stdout}")
            connected.append((source, target))

        time.sleep(arguments.duration_ms / 1000.0)
        if process.poll() is not None:
            raise RuntimeError("candidate process exited during live transaction")
        status = "live-routing-transaction-pass"
    except (OSError, RuntimeError) as failure:
        error = str(failure)
    finally:
        for source, target in reversed(connected):
            command(["pw-link", "-d", source, target], environment)
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    after = links(environment) if before else ""
    restored = bool(before) and before == after
    removed = all(not link_present(after, source, target) for source, target in temporary_links)
    result = {
        "binding": arguments.binding,
        "candidate": "synthv1",
        "duration_ms": arguments.duration_ms,
        "error": error,
        "graph_restored": restored,
        "links_removed": removed,
        "profile": arguments.profile,
        "protected_graph_changed": not restored,
        "schema": "music-studies/s2-live-candidate-bridge/v1",
        "status": status if error is None and restored and removed else "live-routing-transaction-fail",
        "temporary_link_count": len(temporary_links),
        "before_links_sha256": digest(before) if before else None,
        "after_links_sha256": digest(after) if after else None,
    }
    arguments.output.write_text(
        json.dumps(result, ensure_ascii=True, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result, ensure_ascii=True, indent=2, sort_keys=True))
    return 0 if result["status"] == "live-routing-transaction-pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
