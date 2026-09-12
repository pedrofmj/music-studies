#!/usr/bin/env python3
"""Run one reversible candidate profile on the Arturia slot."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import signal
import subprocess
import tempfile
import time
from pathlib import Path


AUDIO_LEFT = "alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__sink:playback_FL"
AUDIO_RIGHT = AUDIO_LEFT[:-2] + "FR"
LIVE_LEFT = "SMC-MIX - 8-Band EQ:Output L"
LIVE_RIGHT = "SMC-MIX - 8-Band EQ:Output R"


def run(command: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        env=environment,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )


def links(environment: dict[str, str]) -> str:
    result = run(["pw-link", "-l"], environment)
    if result.returncode != 0:
        raise RuntimeError(result.stdout)
    return result.stdout


def performance_midi_source(environment: dict[str, str]) -> str:
    result = run(["pw-link", "-o"], environment)
    candidates = [
        line.strip()
        for line in result.stdout.splitlines()
        if "KL Essential 61 mk3" in line
        and "capture_0" in line
        and "MIDI" in line
    ]
    if len(candidates) != 1:
        raise RuntimeError(f"expected one KeyLab performance MIDI source, found {candidates}")
    return candidates[0]


def link_ids(environment: dict[str, str], source: str) -> list[str]:
    result = run(["pw-link", "-l", "-I"], environment)
    return [
        line.split()[0]
        for line in result.stdout.splitlines()
        if "|<-" in line and line.rstrip().endswith(source)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--profile",
        required=True,
        choices=("full-live-rack", "synth-programmer-synthv1", "tonewheel-organ-setbfree"),
    )
    parser.add_argument("--acknowledge-live-routing", action="store_true")
    parser.add_argument("--synthv1", type=Path)
    parser.add_argument("--synthv1-preset", type=Path)
    parser.add_argument("--synthv1-controls", type=Path)
    parser.add_argument("--setbfree", type=Path)
    parser.add_argument("--setbfree-config", type=Path)
    parser.add_argument("--setbfree-library", type=Path)
    parser.add_argument("--duration-ms", type=int, default=5000)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    if arguments.profile == "full-live-rack":
        parser.error("full-live-rack is the protected default/recovery profile; use cleanup to return to it")
    if not arguments.acknowledge_live_routing:
        parser.error("explicit live-routing acknowledgement is required")
    if arguments.duration_ms <= 0:
        parser.error("duration must be positive")

    environment = os.environ.copy()
    environment.setdefault("XDG_RUNTIME_DIR", "/run/user/50001")
    environment.setdefault("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/50001/bus")
    temporary_config = tempfile.TemporaryDirectory(prefix="music-rig-arturia-profile-")
    process: subprocess.Popen[str] | None = None
    mute_process: subprocess.Popen[str] | None = None
    temporary_links: list[tuple[str, str]] = []
    before = ""
    error: str | None = None
    status = "candidate-profile-fail"

    if arguments.profile == "synth-programmer-synthv1":
        required = (arguments.synthv1, arguments.synthv1_preset, arguments.synthv1_controls)
        if any(value is None or not value.is_file() for value in required):
            parser.error("synthv1 profile requires executable, preset, and controls config")
        client = "s2-synthv1-live"
        input_port = f"{client}:in"
        left_port = f"{client}:out_1"
        right_port = f"{client}:out_2"
        command = [
            "/usr/bin/pw-jack", str(arguments.synthv1), "--no-gui",
            "--client-name", client, str(arguments.synthv1_preset),
        ]
        config_home = Path(temporary_config.name) / "config"
        config_file = config_home / "rncbc.org" / "synthv1.conf"
        config_file.parent.mkdir(parents=True)
        shutil.copyfile(arguments.synthv1_controls, config_file)
        environment["XDG_CONFIG_HOME"] = str(config_home)
    else:
        if (
            arguments.setbfree is None
            or not arguments.setbfree.is_file()
            or arguments.setbfree_config is None
            or not arguments.setbfree_config.is_file()
            or arguments.setbfree_library is None
            or not arguments.setbfree_library.is_dir()
        ):
            parser.error("setBfree profile requires binary, config, and library directory")
        client = "setBfree"
        input_port = f"{client}:midi_in"
        left_port = f"{client}:out_left"
        right_port = f"{client}:out_right"
        environment["LD_LIBRARY_PATH"] = str(arguments.setbfree_library)
        command = [
            "/usr/bin/pw-jack", str(arguments.setbfree), "-C", "-c",
            str(arguments.setbfree_config), "jack.connect=",
        ]

    try:
        before = links(environment)
        midi_source = performance_midi_source(environment)
        left_ids = link_ids(environment, LIVE_LEFT)
        right_ids = link_ids(environment, LIVE_RIGHT)
        if len(left_ids) != 1 or len(right_ids) != 1:
            raise RuntimeError("protected live output topology is not exactly one link per channel")
        run(["pw-link", "-d", left_ids[0]], environment)
        run(["pw-link", "-d", right_ids[0]], environment)

        process = subprocess.Popen(
            command,
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            current = links(environment)
            if f"{client}:" in current or run(["pw-link", "-o", left_port], environment).returncode == 0:
                break
            if process.poll() is not None:
                raise RuntimeError("candidate process exited before registering ports")
            time.sleep(0.1)
        else:
            raise RuntimeError("candidate ports did not register")

        for source, target in (
            (midi_source, input_port),
            (left_port, AUDIO_LEFT),
            (right_port, AUDIO_RIGHT),
        ):
            result = run(["pw-link", source, target], environment)
            if result.returncode != 0:
                raise RuntimeError(f"candidate link failed: {source} -> {target}: {result.stdout}")
            temporary_links.append((source, target))

        # The patchbay watcher may restore the protected live links; continuously
        # remove only those two links for this bounded candidate window.
        mute_process = subprocess.Popen(
            ["/bin/sh", "-c", "while :; do "
             "for id in $(pw-link -l -I | awk '/SMC-MIX - 8-Band EQ:Output L$|SMC-MIX - 8-Band EQ:Output R$/ {print $1}'); do "
             "pw-link -d \"$id\" >/dev/null 2>&1 || true; done; sleep 0.1; done"],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        time.sleep(arguments.duration_ms / 1000.0)
        if process.poll() is not None:
            raise RuntimeError("candidate process exited during profile window")
        status = "candidate-profile-pass"
    except (OSError, RuntimeError) as failure:
        error = str(failure)
    finally:
        if mute_process is not None and mute_process.poll() is None:
            mute_process.kill()
            mute_process.wait()
        for source, target in reversed(temporary_links):
            run(["pw-link", "-d", source, target], environment)
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if not any(LIVE_LEFT in line for line in links(environment).splitlines()):
            run(["pw-link", LIVE_LEFT, "Arturia Main Volume Encoder:audio-in-l"], environment)
        if not any(LIVE_RIGHT in line for line in links(environment).splitlines()):
            run(["pw-link", LIVE_RIGHT, "Arturia Main Volume Encoder:audio-in-r"], environment)

    after = links(environment) if before else ""
    result = {
        "profile": arguments.profile,
        "status": status if error is None and before == after else "candidate-profile-fail",
        "error": error,
        "duration_ms": arguments.duration_ms,
        "graph_restored": bool(before) and before == after,
        "protected_graph_changed": bool(before) and before != after,
        "temporary_link_count": len(temporary_links),
        "before_links_sha256": hashlib.sha256(before.encode()).hexdigest() if before else None,
        "after_links_sha256": hashlib.sha256(after.encode()).hexdigest() if after else None,
        "schema": "music-studies/s2-arturia-profile-window/v1",
    }
    arguments.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "candidate-profile-pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
