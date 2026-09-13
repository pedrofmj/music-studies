#!/usr/bin/env python3
"""Run the reversible Arturia pad-driven profile selector."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import tempfile
import time
from pathlib import Path


MIDI_TARGETS = (
    "Arturia Main Volume Encoder:relative-in",
    "AR Controls - Sustain Scale:events-in",
)
LSP_LEFT = "LSP Mixer x8 Stereo:Input L"
LSP_RIGHT = "LSP Mixer x8 Stereo:Input R"
MIDI_GUARD_INTERVAL_SECONDS = 0.5
ARTURIA_AUDIO = (
    ("AR-CH-1 - Basic Piano:output_1", LSP_LEFT),
    ("AR-CH-1 - Basic Piano:output_2", LSP_RIGHT),
    ("AR-CH-2 - Nord White Grand Full 24C:out-left", LSP_LEFT),
    ("AR-CH-2 - Nord White Grand Full 24C:out-right", LSP_RIGHT),
    ("AR-CH-3 - Alt Strings:output_1", LSP_LEFT),
    ("AR-CH-3 - Alt Strings:output_2", LSP_RIGHT),
    ("AR-CH-4 - Good Flute:out-left", LSP_LEFT),
    ("AR-CH-4 - Good Flute:out-right", LSP_RIGHT),
    ("AR-CH-5 - SAX Lirakeys CL:out-left", LSP_LEFT),
    ("AR-CH-5 - SAX Lirakeys CL:out-right", LSP_RIGHT),
    ("AR-CH-6 - Hammond Organ Fast:out-left", LSP_LEFT),
    ("AR-CH-6 - Hammond Organ Fast:out-right", LSP_RIGHT),
    ("AR-CH-7 - Optik Synth:out-left", LSP_LEFT),
    ("AR-CH-7 - Optik Synth:out-right", LSP_RIGHT),
    ("AR-CH-8 - PAD EFEITOS:out-left", LSP_LEFT),
    ("AR-CH-8 - PAD EFEITOS:out-right", LSP_RIGHT),
    ("AR-CH-9 - AtmosferaPAD:out-left", LSP_LEFT),
    ("AR-CH-9 - AtmosferaPAD:out-right", LSP_RIGHT),
)
PAD_PROFILES = {
    1: "tonewheel-organ-setbfree",
    2: "synth-programmer-synthv1",
    3: "full-live-rack",
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
GENRE_ACTIVE_LAYERS = {
    # These are fixed protected Carla engine channels, not logical profile
    # layer numbers. The genre declarations choose the same engine identities.
    "worship-piano": (1, 2, 3, 6, 8, 9),
    "gospel-keys": (1, 2, 3, 5, 6, 7),
    "ambient-worship": (3, 7, 8, 9),
    "jazz-keys": (1, 2, 3, 4, 5, 6),
    "soul-rnb": (1, 2, 3, 5, 6, 7),
    "cinematic-strings": (2, 3, 7, 8, 9),
    "orchestral": (1, 2, 3, 4, 5, 9),
    "brass-winds": (1, 3, 4, 5, 6, 7),
    "synthwave": (1, 2, 3, 7, 8, 9),
    "retro-keys": (1, 2, 6, 7, 8, 9),
    "intimate-pads": (1, 2, 3, 8, 9),
    "praise-leads": (1, 6, 7, 8, 9),
    "acoustic-worship": (1, 2, 3, 4, 6, 9),
}


def run(command: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(command, env=environment, check=False, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, timeout=2.0)
    except subprocess.TimeoutExpired as failure:
        output = failure.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        return subprocess.CompletedProcess(command, 124, output, output)


def links(environment: dict[str, str]) -> str:
    result = run(["pw-link", "-l"], environment)
    if result.returncode != 0:
        raise RuntimeError(result.stdout)
    return result.stdout


def normalized_links(snapshot: str) -> str:
    header = ""
    edges: list[tuple[str, str]] = []
    for line in snapshot.splitlines():
        stripped = line.strip()
        if stripped.startswith("|->"):
            edges.append((header, stripped[3:].strip()))
        elif stripped.startswith("|<-"):
            edges.append((stripped[3:].strip(), header))
        elif stripped:
            header = stripped
    return "\n".join(f"{source}\t{target}" for source, target in sorted(edges))


def link_present(snapshot: str, source: str, target: str) -> bool:
    header = ""
    for line in snapshot.splitlines():
        stripped = line.strip()
        if stripped.startswith("|->"):
            if header == source and stripped[3:].strip() == target:
                return True
        elif stripped.startswith("|<-"):
            if header == target and stripped[3:].strip() == source:
                return True
        elif stripped:
            header = stripped
    return False


def connect(source: str, target: str, environment: dict[str, str]) -> None:
    if link_present(links(environment), source, target):
        return
    result = run(["pw-link", source, target], environment)
    if result.returncode != 0 and "Arquivo existe" not in result.stdout and "File exists" not in result.stdout:
        raise RuntimeError(result.stdout)


def disconnect(source: str, target: str, environment: dict[str, str]) -> None:
    run(["pw-link", "-d", source, target], environment)


def find_keylab(environment: dict[str, str]) -> str:
    result = run(["pw-link", "-o"], environment)
    candidates = [
        line.strip() for line in result.stdout.splitlines()
        if "KL Essential 61 mk3" in line and "capture_0" in line and "MIDI" in line
    ]
    if len(candidates) != 1:
        raise RuntimeError(f"expected one KeyLab MIDI source, found {candidates}")
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--acknowledge-live-routing", action="store_true")
    parser.add_argument("--router", type=Path, required=True)
    parser.add_argument("--synthv1", type=Path, required=True)
    parser.add_argument("--synthv1-preset", type=Path, required=True)
    parser.add_argument("--synthv1-controls", type=Path, required=True)
    parser.add_argument("--setbfree", type=Path, required=True)
    parser.add_argument("--setbfree-config", type=Path, required=True)
    parser.add_argument("--setbfree-library", type=Path, required=True)
    parser.add_argument("--genre-project-root", type=Path)
    parser.add_argument(
        "--control-fifo",
        type=Path,
        default=Path.home() / ".local/state/music-rig/arturia-profile-session/profile.fifo",
    )
    parser.add_argument("--soundfont-workdir", type=Path,
                        default=Path.home() / ".local/share/carla/pedro-soundfonts")
    parser.add_argument("--duration-ms", type=int, default=0,
                        help="milliseconds to run; 0 runs until interrupted")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    if not arguments.acknowledge_live_routing:
        parser.error("explicit live-routing acknowledgement is required")

    environment = os.environ.copy()
    environment.setdefault("XDG_RUNTIME_DIR", "/run/user/50001")
    environment.setdefault("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/50001/bus")
    before = ""
    keylab = ""
    router: subprocess.Popen[str] | None = None
    candidate: subprocess.Popen[str] | None = None
    candidate_instance_id: str | None = None
    config_temporary = tempfile.TemporaryDirectory(prefix="music-rig-arturia-pad-config-")
    fifo = arguments.control_fifo.expanduser()
    fifo_fd = -1
    profiles_seen: list[str] = []
    current = "full-live-rack"
    audio_touched = False
    active_arturia_layers = set(range(1, 10))
    stop_requested = False
    error: str | None = None
    last_midi_guard = 0.0

    def request_stop(signal_number: int, frame: object) -> None:
        nonlocal stop_requested
        del signal_number, frame
        stop_requested = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)
    try:
        before = links(environment)
        keylab = find_keylab(environment)
        fifo.parent.mkdir(parents=True, exist_ok=True)
        if fifo.exists():
            if not fifo.is_fifo():
                raise RuntimeError(f"control FIFO is not a FIFO: {fifo}")
            fifo.unlink()
        os.mkfifo(fifo)
        fifo_fd = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)

        router = subprocess.Popen(
            ["/usr/bin/pw-jack", str(arguments.router), "s2-arturia-profile-router", str(fifo)],
            env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
        )
        time.sleep(2.0)
        for target in MIDI_TARGETS:
            disconnect(keylab, target, environment)
        connect(keylab, "s2-arturia-profile-router:in", environment)
        connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
        connect("s2-arturia-profile-router:out", MIDI_TARGETS[1], environment)

        def stop_candidate() -> None:
            nonlocal candidate, candidate_instance_id
            if candidate_instance_id is not None:
                run(["flatpak", "kill", candidate_instance_id], environment)
                candidate_instance_id = None
            if candidate is not None and candidate.poll() is None:
                candidate.send_signal(signal.SIGTERM)
                try:
                    candidate.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(candidate.pid, signal.SIGKILL)
                    except (PermissionError, ProcessLookupError):
                        candidate.kill()
                    candidate.wait()
            candidate = None

        def restore_live() -> None:
            nonlocal current, audio_touched, active_arturia_layers
            stop_candidate()
            if audio_touched:
                for source, target in ARTURIA_AUDIO:
                    connect(source, target, environment)
                audio_touched = False
            active_arturia_layers = set(range(1, 10))
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
                connect("s2-arturia-profile-router:out", target, environment)
            current = "full-live-rack"

        def start_candidate(profile: str) -> None:
            nonlocal candidate, candidate_instance_id, current, audio_touched, active_arturia_layers
            stop_candidate()
            for source, target in ARTURIA_AUDIO:
                disconnect(source, target, environment)
            audio_touched = True
            active_arturia_layers = set()
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
            if profile == "synth-programmer-synthv1":
                config_home = Path(config_temporary.name) / "synth-config"
                config_file = config_home / "rncbc.org" / "synthv1.conf"
                config_file.parent.mkdir(parents=True, exist_ok=True)
                config_file.write_bytes(arguments.synthv1_controls.read_bytes())
                candidate_environment = dict(environment)
                candidate_environment["XDG_CONFIG_HOME"] = str(config_home)
                command = ["/usr/bin/pw-jack", str(arguments.synthv1), "--no-gui",
                           "--client-name", "s2-synthv1-live", str(arguments.synthv1_preset)]
                candidate = subprocess.Popen(command, env=candidate_environment,
                                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                              text=True, start_new_session=True)
                input_port = "s2-synthv1-live:in"
                left_port = "s2-synthv1-live:out_1"
                right_port = "s2-synthv1-live:out_2"
            elif profile == "tonewheel-organ-setbfree":
                candidate_environment = dict(environment)
                candidate_environment["LD_LIBRARY_PATH"] = str(arguments.setbfree_library)
                candidate = subprocess.Popen(
                    ["/usr/bin/pw-jack", str(arguments.setbfree), "-C", "-c",
                     str(arguments.setbfree_config), "jack.connect="],
                    env=candidate_environment, stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL, text=True, start_new_session=True,
                )
                input_port = "setBfree:midi_in"
                left_port = "setBfree:out_left"
                right_port = "setBfree:out_right"
            elif profile in GENRE_ACTIVE_LAYERS:
                if arguments.genre_project_root is None:
                    raise RuntimeError("genre project root is required")
                project = arguments.genre_project_root / f"{profile}.uproject"
                if not project.is_file():
                    raise RuntimeError(f"genre project is missing: {project}")
                instance_read, instance_write = os.pipe()
                try:
                    candidate = subprocess.Popen(
                        ["/usr/bin/flatpak", "run", f"--cwd={arguments.soundfont_workdir}",
                         "--file-forwarding", f"--instance-id-fd={instance_write}",
                         "studio.kx.carla", "--no-gui", "@@", str(project), "@@"],
                        env=environment, stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL, text=True, start_new_session=True,
                        pass_fds=(instance_write,),
                    )
                finally:
                    os.close(instance_write)
                try:
                    candidate_instance_id = os.read(instance_read, 256).decode().strip()
                finally:
                    os.close(instance_read)
                deadline = time.monotonic() + 20.0
                midi_inputs: list[str] = []
                midi_control_inputs: list[str] = []
                midi_control_outputs: list[str] = []
                audio_outputs: list[str] = []
                while time.monotonic() < deadline:
                    outputs = run(["pw-link", "-o"], environment).stdout.splitlines()
                    inputs = run(["pw-link", "-i"], environment).stdout.splitlines()
                    midi_inputs = [line.strip() for line in inputs
                                   if "GENRE-CH-" in line and ":events-in" in line
                                   and "Volume Map" not in line and "Reverb Map" not in line]
                    midi_control_inputs = [line.strip() for line in inputs
                                           if "GENRE-CH-" in line and ":events-in" in line
                                           and ("Volume Map" in line or "Reverb Map" in line)]
                    midi_control_outputs = [line.strip() for line in outputs
                                            if "GENRE-CH-" in line and ":events-out" in line
                                            and ("Volume Map" in line or "Reverb Map" in line)]
                    audio_outputs = [line.strip() for line in outputs
                                     if "GENRE-CH-" in line and re.search(
                                         r":(?:output_[12]|out-(?:left|right))$",
                                         line.strip(),
                                     )]
                    if (len(midi_inputs) == 9 and len(midi_control_inputs) == 14
                            and len(midi_control_outputs) == 14
                            and len(audio_outputs) == 18):
                        break
                    if candidate.poll() is not None:
                        raise RuntimeError("genre Carla process exited before port registration")
                    time.sleep(0.2)
                else:
                    raise RuntimeError("genre Carla ports did not register")
                for target in MIDI_TARGETS:
                    connect("s2-arturia-profile-router:out", target, environment)
                for target in sorted(midi_inputs):
                    connect("s2-arturia-profile-router:out", target, environment)
                for target in sorted(midi_control_inputs):
                    connect("s2-arturia-profile-router:out", target, environment)
                for output in sorted(midi_control_outputs):
                    channel = output.split(" ", 1)[0]
                    target = next(
                        target for target in midi_inputs
                        if target.startswith(f"{channel} - ")
                    )
                    connect(output, target, environment)
                for output in sorted(audio_outputs):
                    target = LSP_LEFT if any(token in output for token in
                                             (":output_1", ":out-left")) else LSP_RIGHT
                    connect(output, target, environment)
                active_arturia_layers = set()
                current = profile
                profiles_seen.append(profile)
                return
            else:
                restore_live()
                return
            time.sleep(2.0)
            connect("s2-arturia-profile-router:out", input_port, environment)
            connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
            connect(left_port, LSP_LEFT, environment)
            connect(right_port, LSP_RIGHT, environment)
            current = profile
            profiles_seen.append(profile)

        deadline = time.monotonic() + arguments.duration_ms / 1000.0
        while not stop_requested and (arguments.duration_ms == 0 or time.monotonic() < deadline):
            # Keep the management input exclusive while the watcher observes
            # graph events. Normal keyboard/CC data still flows via the router.
            now = time.monotonic()
            if now - last_midi_guard >= MIDI_GUARD_INTERVAL_SECONDS:
                for target in MIDI_TARGETS:
                    disconnect(keylab, target, environment)
                last_midi_guard = now
            if audio_touched:
                for index, (source, target) in enumerate(ARTURIA_AUDIO):
                    if index // 2 + 1 not in active_arturia_layers:
                        disconnect(source, target, environment)
            try:
                profile_code = os.read(fifo_fd, 1)
            except BlockingIOError:
                profile_code = b""
            if profile_code:
                code = profile_code[0]
                profile = PAD_PROFILES.get(code)
                if profile is not None and profile != current:
                    start_candidate(profile)
            time.sleep(0.02)
        restore_live()
    except KeyboardInterrupt:
        error = None
    except (OSError, RuntimeError) as failure:
        error = str(failure)
    finally:
        if fifo_fd >= 0:
            os.close(fifo_fd)
        fifo.unlink(missing_ok=True)
        if candidate_instance_id is not None:
            run(["flatpak", "kill", candidate_instance_id], environment)
            candidate_instance_id = None
        if candidate is not None and candidate.poll() is None:
            candidate.kill()
        if router is not None and router.poll() is None:
            router.send_signal(signal.SIGTERM)
            try:
                router.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                router.kill()
                router.wait()
        if keylab:
            disconnect(keylab, "s2-arturia-profile-router:in", environment)
            for target in MIDI_TARGETS:
                connect(keylab, target, environment)
        for target in MIDI_TARGETS:
            disconnect("s2-arturia-profile-router:out", target, environment)
        if audio_touched:
            for source, target in ARTURIA_AUDIO:
                connect(source, target, environment)

    after = links(environment) if before else ""
    normalized_before = normalized_links(before)
    normalized_after = normalized_links(after)
    result = {
        "profiles_seen": profiles_seen,
        "final_profile": current,
        "duration_ms": arguments.duration_ms,
        "error": error,
        "graph_restored": bool(before) and normalized_before == normalized_after,
        "protected_graph_changed": bool(before) and normalized_before != normalized_after,
        "before_links_sha256": hashlib.sha256(normalized_before.encode()).hexdigest() if before else None,
        "after_links_sha256": hashlib.sha256(normalized_after.encode()).hexdigest() if after else None,
        "schema": "music-studies/s2-arturia-pad-switch/v1",
        "status": "pad-switch-window-pass" if error is None and normalized_before == normalized_after else "pad-switch-window-fail",
    }
    arguments.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "pad-switch-window-pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
