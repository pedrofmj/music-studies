#!/usr/bin/env python3
"""Run the reversible Arturia pad-driven profile selector."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
import datetime
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
ROUTER_INPUT = "s2-arturia-profile-router:in"
LSP_LEFT = "LSP Mixer x8 Stereo:Input L"
LSP_RIGHT = "LSP Mixer x8 Stereo:Input R"
MIDI_GUARD_INTERVAL_SECONDS = 0.5
GENRE_PORT_REGISTRATION_TIMEOUT_SECONDS = 60.0
DEFAULT_PIPEWIRE_QUANTUM = "1024"
GENRE_PIPEWIRE_QUANTUM = "2048"
FULL_CARLA_SERVICE = "music-rig-pedro-carla-headless.service"
CARLA_PROCESS_PATTERN = "^/usr/bin/python3 /app/share/carla/carla "
DEFAULT_DIAGNOSTIC_LOG = Path.home() / ".local/state/music-rig/arturia-profile-session/transition-events.jsonl"
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
MASTER_AUDIO = (
    ("SMC-MIX - 8-Band EQ:Output L", "Arturia Main Volume Encoder:audio-in-l"),
    ("SMC-MIX - 8-Band EQ:Output R", "Arturia Main Volume Encoder:audio-in-r"),
)
MASTER_CONTROL = (
    ("Arturia Main Volume Encoder:absolute-out", "LSP Mixer x8 Stereo:events-in"),
)
SHARED_AUDIO = (
    ("LSP Mixer x8 Stereo:Output L", "SMC-MIX - 8-Band EQ:Input L"),
    ("LSP Mixer x8 Stereo:Output R", "SMC-MIX - 8-Band EQ:Input R"),
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
FAST_ENGINE_PROFILES = {
    "synth-programmer-synthv1": (
        "s2-synthv1-live:in",
        "s2-synthv1-live:out_1",
        "s2-synthv1-live:out_2",
    ),
    "tonewheel-organ-setbfree": (
        "setBfree:midi_in",
        "setBfree:out_left",
        "setBfree:out_right",
    ),
}


@dataclass
class WarmedGenre:
    """A prefixed Carla genre instance and the ports discovered on that instance."""

    profile: str
    prefix: str
    process: subprocess.Popen[str]
    instance_id: str
    midi_inputs: tuple[str, ...]
    midi_control_inputs: tuple[str, ...]
    midi_control_outputs: tuple[str, ...]
    audio_outputs: tuple[str, ...]


def parse_fast_genres(value: str) -> tuple[str, ...]:
    requested = tuple(item.strip() for item in value.split(",") if item.strip())
    if not requested:
        return ()
    if "all" in requested:
        if len(requested) != 1:
            raise ValueError("MUSIC_RIG_FAST_GENRES=all cannot be combined with genre ids")
        return tuple(sorted(GENRE_ACTIVE_LAYERS))
    unknown = sorted(set(requested) - set(GENRE_ACTIVE_LAYERS))
    if unknown:
        raise ValueError(f"unknown fast genre id(s): {', '.join(unknown)}")
    return tuple(dict.fromkeys(requested))


def run(command: list[str], environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(command, env=environment, check=False, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, timeout=2.0)
    except subprocess.TimeoutExpired as failure:
        output = failure.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        return subprocess.CompletedProcess(command, 124, output, output)


def diagnostic_event(path: Path, event: str, **fields: object) -> None:
    payload = {
        "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "event": event,
        "pid": os.getpid(),
        **fields,
    }
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(payload, sort_keys=True) + "\n")
    except OSError:
        pass


def links(environment: dict[str, str]) -> str:
    result = run(["pw-link", "-l"], environment)
    if result.returncode != 0:
        raise RuntimeError(result.stdout)
    return result.stdout


def set_pipewire_quantum(value: str, environment: dict[str, str]) -> None:
    run(["pw-metadata", "-n", "settings", "0", "clock.force-quantum", value], environment)


def systemd_user(action: str, unit: str, environment: dict[str, str]) -> None:
    result = run(["systemctl", "--user", action, unit], environment)
    if result.returncode != 0:
        raise RuntimeError(result.stdout)


def stop_systemd_user(unit: str, environment: dict[str, str]) -> None:
    run(["systemctl", "--user", "stop", unit], environment)


def wait_for_carla(present: bool, environment: dict[str, str], timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        running = run(["pgrep", "-f", CARLA_PROCESS_PATTERN], environment).returncode == 0
        if running == present:
            return
        time.sleep(0.25)
    state = "register" if present else "exit"
    raise RuntimeError(f"Carla did not {state} within {timeout:g} seconds")


def wait_for_service_stopped(environment: dict[str, str], timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if run(["systemctl", "--user", "is-active", FULL_CARLA_SERVICE], environment).returncode != 0:
            return
        time.sleep(0.25)
    raise RuntimeError(f"Carla service did not stop within {timeout:g} seconds")


def terminate_carla_processes(environment: dict[str, str]) -> None:
    for signal_number in (signal.SIGTERM, signal.SIGKILL):
        result = run(["pgrep", "-f", CARLA_PROCESS_PATTERN], environment)
        for value in result.stdout.split():
            try:
                os.kill(int(value), signal_number)
            except (ProcessLookupError, PermissionError, ValueError):
                pass
        try:
            wait_for_carla(False, environment, timeout=8.0)
            return
        except RuntimeError:
            continue
    raise RuntimeError("Carla backend could not be terminated")


def cleanup_orphaned_warm_genres(environment: dict[str, str]) -> None:
    """Remove only prefixed warm genre Carla processes left by an old session."""
    result = run(["ps", "-eo", "pid=,args="], environment)
    pids = []
    for line in result.stdout.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) == 2 and "FAST-GENRE-" in parts[1]:
            try:
                pids.append(int(parts[0]))
            except ValueError:
                continue
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
    if pids:
        time.sleep(1.0)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass


def wait_for_ports(tokens: tuple[str, ...], environment: dict[str, str], timeout: float = 45.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        outputs = run(["pw-link", "-o"], environment).stdout
        inputs = run(["pw-link", "-i"], environment).stdout
        if all(token in outputs or token in inputs for token in tokens):
            return
        time.sleep(0.25)
    raise RuntimeError(f"audio base ports did not register: {tokens}")


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


def wait_for_links_absent(connections: tuple[tuple[str, str], ...],
                          environment: dict[str, str], timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        snapshot = links(environment)
        if all(not link_present(snapshot, source, target) for source, target in connections):
            return
        time.sleep(0.05)
    raise RuntimeError("PipeWire links did not disconnect before the next route was connected")


def connect(source: str, target: str, environment: dict[str, str]) -> None:
    deadline = time.monotonic() + 15.0
    last_error = ""
    while time.monotonic() < deadline:
        if link_present(links(environment), source, target):
            return
        result = run(["pw-link", source, target], environment)
        if result.returncode == 0 or "Arquivo existe" in result.stdout or "File exists" in result.stdout:
            return
        last_error = result.stdout
        time.sleep(0.25)
    raise RuntimeError(f"failed to link {source} -> {target}: {last_error}")


def connect_many(connections: tuple[tuple[str, str], ...], environment: dict[str, str]) -> None:
    output_ids = port_ids("-o", environment)
    input_ids = port_ids("-i", environment)

    def connect_pair(source: str, target: str) -> None:
        for attempt in range(2):
            source_id = output_ids.get(source)
            target_id = input_ids.get(target)
            if source_id is not None and target_id is not None:
                result = run(["pw-link", str(source_id), str(target_id)], environment)
                if result.returncode == 0 or "File exists" in result.stdout or "Arquivo existe" in result.stdout:
                    return
            if attempt == 0:
                output_ids.update(port_ids("-o", environment))
                input_ids.update(port_ids("-i", environment))
        connect(source, target, environment)

    with ThreadPoolExecutor(max_workers=min(8, len(connections))) as executor:
        futures = [executor.submit(connect_pair, source, target)
                   for source, target in connections]
        for future in futures:
            future.result()


def port_ids(direction: str, environment: dict[str, str]) -> dict[str, int]:
    result = run(["pw-link", "-I", direction], environment)
    ports: dict[str, int] = {}
    for line in result.stdout.splitlines():
        parts = line.strip().split(None, 1)
        if len(parts) != 2 or not parts[0].isdigit():
            continue
        ports[parts[1]] = int(parts[0])
    return ports


def connect_by_current_ids(source: str, target: str, environment: dict[str, str]) -> None:
    last_output_id = None
    last_input_id = None
    for attempt in range(4):
        output_ids = port_ids("-o", environment)
        input_ids = port_ids("-i", environment)
        last_output_id = output_ids.get(source)
        last_input_id = input_ids.get(target)
        if connect_with_ids(source, target, output_ids, input_ids, environment):
            return
        if attempt < 3:
            time.sleep(0.25)
    raise RuntimeError(
        f"warm port link failed for {source} -> {target}; "
        f"resolved ids output={last_output_id} input={last_input_id}"
    )


def connect_with_ids(source: str, target: str, output_ids: dict[str, int],
                     input_ids: dict[str, int], environment: dict[str, str]) -> bool:
    output_id = output_ids.get(source)
    input_id = input_ids.get(target)
    if output_id is not None and input_id is not None:
        result = run(["pw-link", str(output_id), str(input_id)], environment)
        if result.returncode == 0 or "File exists" in result.stdout or "Arquivo existe" in result.stdout:
            return True
    return False
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
    parser.add_argument(
        "--transition-mode",
        choices=("safe", "fast"),
        default=os.environ.get("MUSIC_RIG_TRANSITION_MODE", "safe"),
        help="safe rebuilds engines per transition; fast keeps SynthV1 and setBfree warm",
    )
    parser.epilog = (
        "Fast genre prewarming is opt-in: set MUSIC_RIG_FAST_GENRES to a comma-separated "
        "list of genre ids (for example worship-piano,jazz-keys) or all. It is used only "
        "with --transition-mode fast; the default is no genre prewarming."
    )
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
    parser.add_argument("--diagnostic-log", type=Path, default=DEFAULT_DIAGNOSTIC_LOG)
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
    diagnostic_log = arguments.diagnostic_log.expanduser()
    diagnostic_event(diagnostic_log, "session-start", output=str(arguments.output))
    before = ""
    keylab = ""
    router: subprocess.Popen[str] | None = None
    candidate: subprocess.Popen[str] | None = None
    candidate_instance_id: str | None = None
    config_temporary = tempfile.TemporaryDirectory(prefix="music-rig-arturia-pad-config-")
    fifo = arguments.control_fifo.expanduser()
    fifo_fd = -1
    profiles_seen: list[str] = []
    transition_failures: list[dict[str, str]] = []
    current = "full-live-rack"
    audio_touched = False
    active_arturia_layers = set(range(1, 10))
    fast_mode_enabled = arguments.transition_mode == "fast"
    warm_candidates: dict[str, subprocess.Popen[str]] = {}
    warmed_genres: dict[str, WarmedGenre] = {}
    fast_genre_enabled = False
    configured_fast_genres: tuple[str, ...] = ()
    stop_requested = False
    error: str | None = None
    last_midi_guard = 0.0
    genre_quantum_changed = False

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

        systemd_user("start", FULL_CARLA_SERVICE, environment)
        wait_for_carla(True, environment)
        wait_for_ports(("AR-CH-1 - Basic Piano:output_1", "SMC-MIX - 8-Band EQ:Output L",
                        "Arturia Main Volume Encoder:relative-in",
                        "AR Controls - Sustain Scale:events-in"), environment)
        router = subprocess.Popen(
            ["/usr/bin/pw-jack", str(arguments.router), "s2-arturia-profile-router", str(fifo)],
            env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
        )
        wait_for_ports(("s2-arturia-profile-router:in", "s2-arturia-profile-router:out",
                        *MIDI_TARGETS), environment)
        for target in MIDI_TARGETS:
            disconnect(keylab, target, environment)
        connect(keylab, ROUTER_INPUT, environment)
        connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
        connect("s2-arturia-profile-router:out", MIDI_TARGETS[1], environment)

        def stop_candidate() -> None:
            nonlocal candidate, candidate_instance_id, current
            was_genre_candidate = current in GENRE_ACTIVE_LAYERS or candidate_instance_id is not None
            preserve_warm_genres = bool(warmed_genres)
            if current in warmed_genres:
                disconnect_warmed_genre(warmed_genres[current])
            if candidate is not None:
                was_genre_candidate = was_genre_candidate or any(
                    "genre-projects" in str(argument) for argument in candidate.args
                )
            if candidate_instance_id is not None:
                run(["flatpak", "kill", candidate_instance_id], environment)
                if not preserve_warm_genres:
                    run(["flatpak", "kill", "studio.kx.carla"], environment)
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
            full_audio_active = run(
                ["systemctl", "--user", "is-active", FULL_CARLA_SERVICE], environment
            ).returncode == 0
            if was_genre_candidate and not preserve_warm_genres and not full_audio_active:
                terminate_carla_processes(environment)

        def start_synth_engine() -> subprocess.Popen[str]:
            config_home = Path(config_temporary.name) / "synth-config"
            config_file = config_home / "rncbc.org" / "synthv1.conf"
            config_file.parent.mkdir(parents=True, exist_ok=True)
            config_file.write_bytes(arguments.synthv1_controls.read_bytes())
            candidate_environment = dict(environment)
            candidate_environment["XDG_CONFIG_HOME"] = str(config_home)
            process = subprocess.Popen(
                ["/usr/bin/pw-jack", str(arguments.synthv1), "--no-gui",
                 "--client-name", "s2-synthv1-live", str(arguments.synthv1_preset)],
                env=candidate_environment, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, text=True, start_new_session=True,
            )
            wait_for_ports(FAST_ENGINE_PROFILES["synth-programmer-synthv1"], environment)
            return process

        def start_setbfree_engine() -> subprocess.Popen[str]:
            candidate_environment = dict(environment)
            candidate_environment["LD_LIBRARY_PATH"] = str(arguments.setbfree_library)
            process = subprocess.Popen(
                ["/usr/bin/pw-jack", str(arguments.setbfree), "-C", "-c",
                 str(arguments.setbfree_config), "jack.connect="],
                env=candidate_environment, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, text=True, start_new_session=True,
            )
            wait_for_ports(FAST_ENGINE_PROFILES["tonewheel-organ-setbfree"], environment)
            return process

        def discover_warmed_genre_ports(
            prefix: str, process: subprocess.Popen[str]
        ) -> tuple[tuple[str, ...], tuple[str, ...], tuple[str, ...], tuple[str, ...]]:
            deadline = time.monotonic() + GENRE_PORT_REGISTRATION_TIMEOUT_SECONDS
            while time.monotonic() < deadline:
                outputs = run(["pw-link", "-o"], environment).stdout.splitlines()
                inputs = run(["pw-link", "-i"], environment).stdout.splitlines()
                midi_inputs = tuple(sorted(
                    line.strip() for line in inputs
                    if line.strip().startswith(prefix) and "/GENRE-CH-" in line
                    and ":events-in" in line
                    and "Volume Map" not in line
                    and "Reverb Map" not in line
                ))
                midi_control_inputs = tuple(sorted(
                    line.strip() for line in inputs
                    if line.strip().startswith(prefix) and "/GENRE-CH-" in line
                    and ":events-in" in line
                    and ("Volume Map" in line or "Reverb Map" in line)
                ))
                midi_control_outputs = tuple(sorted(
                    line.strip() for line in outputs
                    if line.strip().startswith(prefix) and "/GENRE-CH-" in line
                    and ":events-out" in line
                    and ("Volume Map" in line or "Reverb Map" in line)
                ))
                audio_outputs = tuple(sorted(
                    line.strip() for line in outputs
                    if line.strip().startswith(prefix) and "/GENRE-CH-" in line
                    and re.search(r":(?:output_[12]|out-(?:left|right))$", line.strip())
                ))
                if (len(midi_inputs) == 9 and len(midi_control_inputs) == 14
                        and len(midi_control_outputs) == 14 and len(audio_outputs) == 18):
                    return (
                        midi_inputs,
                        midi_control_inputs,
                        midi_control_outputs,
                        audio_outputs,
                    )
                if process.poll() is not None:
                    raise RuntimeError("warmed genre Carla exited before port registration")
                time.sleep(0.2)
            raise RuntimeError(f"warmed genre ports did not register for {prefix}")

        def start_warm_genre(profile: str) -> WarmedGenre:
            if arguments.genre_project_root is None:
                raise RuntimeError("genre project root is required for fast genre prewarming")
            project = arguments.genre_project_root / f"{profile}.uproject"
            if not project.is_file():
                raise RuntimeError(f"genre project is missing: {project}")
            prefix = f"FAST-GENRE-{profile}-"
            instance_read, instance_write = os.pipe()
            process: subprocess.Popen[str] | None = None
            instance_id = ""
            try:
                process = subprocess.Popen(
                    ["/usr/bin/flatpak", "run", f"--cwd={arguments.soundfont_workdir}",
                     "--file-forwarding", f"--instance-id-fd={instance_write}",
                     "studio.kx.carla", "--no-gui", f"--cnprefix={prefix}",
                     "@@", str(project), "@@"],
                    env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                    text=True, start_new_session=True, pass_fds=(instance_write,),
                )
            finally:
                os.close(instance_write)
            os.set_blocking(instance_read, False)
            try:
                try:
                    instance_deadline = time.monotonic() + GENRE_PORT_REGISTRATION_TIMEOUT_SECONDS
                    while time.monotonic() < instance_deadline:
                        try:
                            instance_id = os.read(instance_read, 256).decode().strip()
                        except BlockingIOError:
                            if process.poll() is not None:
                                raise RuntimeError("warmed genre Carla exited before registering")
                            time.sleep(0.2)
                            continue
                        if instance_id:
                            break
                    else:
                        raise RuntimeError("warmed genre Carla instance did not register")
                finally:
                    os.close(instance_read)
                ports = discover_warmed_genre_ports(prefix, process)
                time.sleep(1.0)
                return WarmedGenre(profile, prefix, process, instance_id, *ports)
            except (OSError, RuntimeError):
                run(["flatpak", "kill", instance_id], environment)
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
                raise

        def genre_midi_connections(warm: WarmedGenre) -> tuple[tuple[str, str], ...]:
            active_layers = GENRE_ACTIVE_LAYERS[warm.profile]
            active_marker = tuple(f"/GENRE-CH-{layer} " for layer in active_layers)

            def active(port: str) -> bool:
                return any(marker in port for marker in active_marker)

            connections: list[tuple[str, str]] = [
                ("s2-arturia-profile-router:out", target)
                for target in (*warm.midi_inputs, *warm.midi_control_inputs)
                if active(target)
            ]
            for output in warm.midi_control_outputs:
                if not active(output):
                    continue
                channel = output.split(" ", 1)[0]
                target = next(
                    (target for target in warm.midi_inputs
                     if target.startswith(f"{channel} - ")),
                    None,
                )
                if target is None:
                    raise RuntimeError(f"no MIDI input for warmed genre control port: {output}")
                connections.append((output, target))
            return tuple(connections)

        def genre_audio_connections(warm: WarmedGenre) -> tuple[tuple[str, str], ...]:
            active_layers = GENRE_ACTIVE_LAYERS[warm.profile]
            return tuple(
                (output, genre_audio_target(output, warm.profile))
                for output in warm.audio_outputs
                if any(f"/GENRE-CH-{layer} " in output for layer in active_layers)
            )

        def genre_audio_target(output: str, profile: str) -> str:
            match = re.search(r"(?:^|/)GENRE-CH-(\d+) ", output)
            if match is None:
                raise RuntimeError(f"could not identify warmed genre audio channel: {output}")
            channel = int(match.group(1))
            active_layers = GENRE_ACTIVE_LAYERS[profile]
            if channel not in active_layers:
                raise RuntimeError(f"inactive genre channel was routed: {output}")
            slot = active_layers.index(channel) + 1
            side = "left" if any(token in output for token in (":output_1", ":out-left")) else "right"
            return f"LSP Mixer x8 Stereo:Audio input {side} {slot}"

        def connect_warmed_genre(warm: WarmedGenre) -> None:
            refresh_warmed_genre_ports(warm)
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
            midi_connections = genre_midi_connections(warm)
            router_connections = [connection for connection in midi_connections
                                  if connection[0] == "s2-arturia-profile-router:out"]
            other_midi_connections = [connection for connection in midi_connections
                                      if connection[0] != "s2-arturia-profile-router:out"]
            output_ids = port_ids("-o", environment)
            input_ids = port_ids("-i", environment)

            def connect_cached(source: str, target: str) -> None:
                if connect_with_ids(source, target, output_ids, input_ids, environment):
                    return
                connect_by_current_ids(source, target, environment)

            for source, target in genre_audio_connections(warm):
                connect_cached(source, target)
            connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
            for source, target in router_connections:
                connect_cached(source, target)
            for source, target in other_midi_connections:
                connect_cached(source, target)
            expected = (*genre_midi_connections(warm), *genre_audio_connections(warm))
            current_links = links(environment)
            missing = [(source, target) for source, target in expected
                       if not link_present(current_links, source, target)]
            if missing:
                raise RuntimeError(f"warmed genre route validation failed: {missing[0][0]} -> {missing[0][1]}")

        def validate_warmed_genre(warm: WarmedGenre) -> None:
            refresh_warmed_genre_ports(warm)
            connections = (
                ("s2-arturia-profile-router:out", MIDI_TARGETS[0]),
                *genre_midi_connections(warm),
                *genre_audio_connections(warm),
                *SHARED_AUDIO,
                *MASTER_AUDIO,
            )
            outputs = port_ids("-o", environment)
            inputs = port_ids("-i", environment)
            missing = [
                f"{source} -> {target}"
                for source, target in connections
                if source not in outputs or target not in inputs
            ]
            if missing:
                raise RuntimeError(f"warmed genre endpoints missing before switch: {missing[0]}")

        def refresh_warmed_genre_ports(warm: WarmedGenre) -> None:
            if warm.process.poll() is not None:
                raise RuntimeError(f"warmed genre process exited: {warm.profile}")
            ports = discover_warmed_genre_ports(warm.prefix, warm.process)
            warm.midi_inputs, warm.midi_control_inputs, warm.midi_control_outputs, warm.audio_outputs = ports

        def disconnect_warmed_genre(warm: WarmedGenre) -> None:
            for source, target in (*genre_midi_connections(warm), *genre_audio_connections(warm)):
                disconnect(source, target, environment)
            for source, target in (*SHARED_AUDIO, *MASTER_AUDIO):
                disconnect(source, target, environment)
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)

        def stop_warm_genres() -> None:
            for warm in list(warmed_genres.values()):
                run(["flatpak", "kill", warm.instance_id], environment)
                if warm.process.poll() is None:
                    warm.process.send_signal(signal.SIGTERM)
                    try:
                        warm.process.wait(timeout=3.0)
                    except subprocess.TimeoutExpired:
                        try:
                            os.killpg(warm.process.pid, signal.SIGKILL)
                        except (PermissionError, ProcessLookupError):
                            warm.process.kill()
                        warm.process.wait()
            warmed_genres.clear()

        def start_warm_genres() -> None:
            try:
                for profile in configured_fast_genres:
                    warmed_genres[profile] = start_warm_genre(profile)
            except (OSError, RuntimeError):
                stop_warm_genres()
                raise

        def stop_warm_engines() -> None:
            for process in list(warm_candidates.values()):
                if process.poll() is not None:
                    continue
                process.send_signal(signal.SIGTERM)
                try:
                    process.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except (PermissionError, ProcessLookupError):
                        process.kill()
                    process.wait()
            warm_candidates.clear()

        def ensure_router() -> None:
            nonlocal router, keylab
            outputs = run(["pw-link", "-o"], environment).stdout
            inputs = run(["pw-link", "-i"], environment).stdout
            if (router is not None and router.poll() is None
                    and "s2-arturia-profile-router:out" in outputs
                    and "s2-arturia-profile-router:in" in inputs):
                return
            if router is not None and router.poll() is None:
                router.send_signal(signal.SIGTERM)
                try:
                    router.wait(timeout=3.0)
                except subprocess.TimeoutExpired:
                    router.kill()
                    router.wait()
            router = subprocess.Popen(
                ["/usr/bin/pw-jack", str(arguments.router), "s2-arturia-profile-router", str(fifo)],
                env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, text=True,
            )
            wait_for_ports(("s2-arturia-profile-router:in", "s2-arturia-profile-router:out"), environment)
            keylab = find_keylab(environment)
            for target in MIDI_TARGETS:
                disconnect(keylab, target, environment)
            connect(keylab, ROUTER_INPUT, environment)

        def start_warm_engines() -> None:
            try:
                warm_candidates["synth-programmer-synthv1"] = start_synth_engine()
                warm_candidates["tonewheel-organ-setbfree"] = start_setbfree_engine()
            except (OSError, RuntimeError):
                stop_warm_engines()
                raise

        def fast_switch_engine(profile: str) -> None:
            nonlocal current, audio_touched, active_arturia_layers, genre_quantum_changed
            if profile not in FAST_ENGINE_PROFILES:
                raise RuntimeError(f"Fast mode does not support profile: {profile}")
            if profile not in warm_candidates or warm_candidates[profile].poll() is not None:
                raise RuntimeError(f"Warm engine is not running: {profile}")
            was_warmed_genre = current in warmed_genres
            if was_warmed_genre:
                stop_candidate()
            if current == "full-live-rack" and not audio_touched:
                for source, target in ARTURIA_AUDIO:
                    disconnect(source, target, environment)
                wait_for_links_absent(ARTURIA_AUDIO, environment)
                audio_touched = True
            for engine_input, left_output, right_output in FAST_ENGINE_PROFILES.values():
                disconnect("s2-arturia-profile-router:out", engine_input, environment)
                disconnect(left_output, LSP_LEFT, environment)
                disconnect(right_output, LSP_RIGHT, environment)
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
            engine_input, left_output, right_output = FAST_ENGINE_PROFILES[profile]
            connect("s2-arturia-profile-router:out", engine_input, environment)
            connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
            connect(left_output, LSP_LEFT, environment)
            connect(right_output, LSP_RIGHT, environment)
            if was_warmed_genre:
                connect_many(SHARED_AUDIO, environment)
                connect_many(MASTER_AUDIO, environment)
                if genre_quantum_changed:
                    set_pipewire_quantum(DEFAULT_PIPEWIRE_QUANTUM, environment)
                    genre_quantum_changed = False
            active_arturia_layers = set()
            current = profile
            profiles_seen.append(profile)

        def fast_switch_genre(profile: str) -> None:
            nonlocal current, audio_touched, active_arturia_layers, genre_quantum_changed
            warm = warmed_genres.get(profile)
            if warm is None or warm.process.poll() is not None:
                raise RuntimeError(f"Warmed genre is not running: {profile}")
            if not genre_quantum_changed:
                set_pipewire_quantum(GENRE_PIPEWIRE_QUANTUM, environment)
                genre_quantum_changed = True
            validate_warmed_genre(warm)
            if current in GENRE_ACTIVE_LAYERS:
                stop_candidate()
            if current == "full-live-rack" and not audio_touched:
                for source, target in ARTURIA_AUDIO:
                    disconnect(source, target, environment)
                wait_for_links_absent(ARTURIA_AUDIO, environment)
                audio_touched = True
            for engine_input, left_output, right_output in FAST_ENGINE_PROFILES.values():
                disconnect("s2-arturia-profile-router:out", engine_input, environment)
                disconnect(left_output, LSP_LEFT, environment)
                disconnect(right_output, LSP_RIGHT, environment)
            if current in warmed_genres:
                disconnect_warmed_genre(warmed_genres[current])
            connect_warmed_genre(warm)
            active_arturia_layers = set()
            current = profile
            profiles_seen.append(profile)

        def fast_restore_full() -> None:
            nonlocal current, audio_touched, active_arturia_layers, genre_quantum_changed
            ensure_router()
            for engine_input, left_output, right_output in FAST_ENGINE_PROFILES.values():
                disconnect("s2-arturia-profile-router:out", engine_input, environment)
                disconnect(left_output, LSP_LEFT, environment)
                disconnect(right_output, LSP_RIGHT, environment)
            if current in warmed_genres:
                disconnect_warmed_genre(warmed_genres[current])
            ensure_full_audio(restore_independent=False)
            restore_fast_independent_device_routes()
            if not independent_routes_present():
                restore_independent_device_routes()
            if genre_quantum_changed:
                set_pipewire_quantum(DEFAULT_PIPEWIRE_QUANTUM, environment)
                genre_quantum_changed = False
            if audio_touched:
                connect_many(ARTURIA_AUDIO, environment)
                audio_touched = False
            active_arturia_layers = set(range(1, 10))
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
                connect("s2-arturia-profile-router:out", target, environment)
            for source, target in MASTER_CONTROL:
                connect(source, target, environment)
            current = "full-live-rack"

        def restore_independent_device_routes() -> None:
            tool = Path.home() / "bin/pipewire-patchbay-json"
            if tool.is_file():
                try:
                    subprocess.run(
                        [str(tool), "--check-and-restore"],
                        env=environment,
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        timeout=20.0,
                    )
                except subprocess.TimeoutExpired:
                    diagnostic_event(diagnostic_log, "independent-route-restore-timeout")

        def restore_fast_independent_device_routes() -> None:
            outputs = run(["pw-link", "-o"], environment).stdout.splitlines()
            inputs = run(["pw-link", "-i"], environment).stdout
            targets = {
                "SMC-Mixer-Master": tuple(
                    f"SMC-EQ-{channel} CC Scale:events-in" for channel in range(1, 9)
                ),
                "SMC-PAD-Master": (
                    "PD Controls - Sustain Scale:events-in",
                    "PD-CH-1 - Drum Set:events-in",
                    "PD-CH-1 Volume Map:events-in",
                    "PD-CH-1 Gain Map:events-in",
                ),
                "SMC-PAD Pocket-Master": ("PD-CH-1 - Drum Set:events-in",),
                "SMK25-Master": ("SMK25 Pad Layers:midi-in",),
            }
            for alias, destinations in targets.items():
                source = next((line.strip() for line in outputs
                               if alias in line and "capture_1" in line), None)
                if source is None:
                    continue
                for destination in destinations:
                    if destination in inputs:
                        connect(source, destination, environment)

        def independent_routes_present() -> bool:
            snapshot = links(environment)
            return all(alias in snapshot for alias in (
                "SMK25-Master", "SMC-PAD-Master", "SMC-Mixer-Master"))

        def ensure_full_audio(restore_independent: bool = True) -> None:
            systemd_user("start", FULL_CARLA_SERVICE, environment)
            wait_for_carla(True, environment)
            wait_for_ports(("AR-CH-1 - Basic Piano:output_1", "SMC-MIX - 8-Band EQ:Output L"), environment)
            connect_many(MASTER_AUDIO, environment)
            connect_many(MASTER_CONTROL, environment)
            connect_many(SHARED_AUDIO, environment)
            if restore_independent:
                restore_independent_device_routes()

        def restore_live() -> None:
            nonlocal current, audio_touched, active_arturia_layers, genre_quantum_changed
            stop_candidate()
            ensure_router()
            ensure_full_audio()
            if genre_quantum_changed:
                set_pipewire_quantum(DEFAULT_PIPEWIRE_QUANTUM, environment)
                genre_quantum_changed = False
            if audio_touched:
                connect_many(ARTURIA_AUDIO, environment)
                audio_touched = False
            active_arturia_layers = set(range(1, 10))
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
                connect("s2-arturia-profile-router:out", target, environment)
            for source, target in MASTER_CONTROL:
                connect(source, target, environment)
            current = "full-live-rack"

        def start_candidate(profile: str) -> None:
            nonlocal candidate, candidate_instance_id, current, audio_touched
            nonlocal active_arturia_layers, genre_quantum_changed
            if fast_mode_enabled and profile in FAST_ENGINE_PROFILES:
                if current == "full-live-rack" or current in FAST_ENGINE_PROFILES or current in warmed_genres:
                    fast_switch_engine(profile)
                    return
            if fast_genre_enabled and profile in warmed_genres:
                fast_switch_genre(profile)
                return
            if fast_mode_enabled and profile == "full-live-rack" and (
                    current in FAST_ENGINE_PROFILES or current in warmed_genres):
                fast_restore_full()
                return
            was_genre = current in GENRE_ACTIVE_LAYERS
            stop_candidate()
            if profile in GENRE_ACTIVE_LAYERS:
                # Headless Carla owns the shared LSP/SMC mixer and output path.
                # Genre Carla supplies only the temporary instrument layer.
                if run(["systemctl", "--user", "is-active", FULL_CARLA_SERVICE],
                       environment).returncode != 0:
                    ensure_full_audio()
            else:
                ensure_full_audio()
            if profile in GENRE_ACTIVE_LAYERS:
                set_pipewire_quantum(GENRE_PIPEWIRE_QUANTUM, environment)
                genre_quantum_changed = True
            elif genre_quantum_changed:
                set_pipewire_quantum(DEFAULT_PIPEWIRE_QUANTUM, environment)
                genre_quantum_changed = False
            for source, target in ARTURIA_AUDIO:
                disconnect(source, target, environment)
            audio_touched = True
            active_arturia_layers = set()
            for target in MIDI_TARGETS:
                disconnect("s2-arturia-profile-router:out", target, environment)
            if profile in GENRE_ACTIVE_LAYERS:
                connect("s2-arturia-profile-router:out", MIDI_TARGETS[0], environment)
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
                os.set_blocking(instance_read, False)
                try:
                    instance_deadline = time.monotonic() + GENRE_PORT_REGISTRATION_TIMEOUT_SECONDS
                    while time.monotonic() < instance_deadline:
                        try:
                            candidate_instance_id = os.read(instance_read, 256).decode().strip()
                        except BlockingIOError:
                            if candidate.poll() is not None:
                                raise RuntimeError("genre Carla exited before registering")
                            time.sleep(0.2)
                            continue
                        if candidate_instance_id:
                            break
                    else:
                        raise RuntimeError("genre Carla instance did not register")
                finally:
                    os.close(instance_read)
                deadline = time.monotonic() + GENRE_PORT_REGISTRATION_TIMEOUT_SECONDS
                midi_inputs: list[str] = []
                midi_control_inputs: list[str] = []
                midi_control_outputs: list[str] = []
                audio_outputs: list[str] = []
                while time.monotonic() < deadline:
                    outputs = run(["pw-link", "-o"], environment).stdout.splitlines()
                    inputs = run(["pw-link", "-i"], environment).stdout.splitlines()
                    warm_prefixes = tuple(warm.prefix for warm in warmed_genres.values())
                    midi_inputs = [line.strip() for line in inputs
                                   if "GENRE-CH-" in line and ":events-in" in line
                                   and not line.strip().startswith(warm_prefixes)
                                   and "Volume Map" not in line and "Reverb Map" not in line]
                    midi_control_inputs = [line.strip() for line in inputs
                                           if "GENRE-CH-" in line and ":events-in" in line
                                           and not line.strip().startswith(warm_prefixes)
                                           and ("Volume Map" in line or "Reverb Map" in line)]
                    midi_control_outputs = [line.strip() for line in outputs
                                            if "GENRE-CH-" in line and ":events-out" in line
                                            and not line.strip().startswith(warm_prefixes)
                                            and ("Volume Map" in line or "Reverb Map" in line)]
                    audio_outputs = [line.strip() for line in outputs
                                     if "GENRE-CH-" in line and re.search(
                                         r":(?:output_[12]|out-(?:left|right))$",
                                         line.strip(),
                                     )]
                    audio_outputs = [line for line in audio_outputs
                                     if not line.startswith(warm_prefixes)]
                    if (len(midi_inputs) == 9 and len(midi_control_inputs) == 14
                            and len(midi_control_outputs) == 14
                            and len(audio_outputs) == 18):
                        break
                    if candidate.poll() is not None:
                        raise RuntimeError("genre Carla process exited before port registration")
                    time.sleep(0.2)
                else:
                    raise RuntimeError("genre Carla ports did not register")
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
                active_layers = GENRE_ACTIVE_LAYERS[profile]
                for output in sorted(audio_outputs):
                    if any(f"GENRE-CH-{layer} " in output for layer in active_layers):
                        connect(output, genre_audio_target(output, profile), environment)
                for source, target in SHARED_AUDIO:
                    connect(source, target, environment)
                for source, target in MASTER_AUDIO:
                    connect(source, target, environment)
                restore_independent_device_routes()
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

        ensure_full_audio()
        if fast_mode_enabled:
            cleanup_orphaned_warm_genres(environment)
            try:
                start_warm_engines()
                diagnostic_event(diagnostic_log, "fast-mode-enabled", warm_profiles=sorted(warm_candidates))
            except (OSError, RuntimeError) as failure:
                fast_mode_enabled = False
                diagnostic_event(diagnostic_log, "fast-mode-fallback", error=str(failure))
            if fast_mode_enabled and os.environ.get("MUSIC_RIG_FAST_GENRES", "").strip():
                try:
                    configured_fast_genres = parse_fast_genres(
                        os.environ["MUSIC_RIG_FAST_GENRES"]
                    )
                    if configured_fast_genres:
                        start_warm_genres()
                        fast_genre_enabled = True
                        diagnostic_event(
                            diagnostic_log,
                            "fast-mode-enabled",
                            warm_genres=list(configured_fast_genres),
                        )
                except (OSError, RuntimeError, ValueError) as failure:
                    fast_genre_enabled = False
                    stop_warm_genres()
                    diagnostic_event(diagnostic_log, "fast-mode-fallback", error=str(failure))
        deadline = time.monotonic() + arguments.duration_ms / 1000.0
        while not stop_requested and (arguments.duration_ms == 0 or time.monotonic() < deadline):
            # Keep the management input exclusive while the watcher observes
            # graph events. Normal keyboard/CC data still flows via the router.
            now = time.monotonic()
            if now - last_midi_guard >= MIDI_GUARD_INTERVAL_SECONDS:
                try:
                    keylab = find_keylab(environment)
                except RuntimeError:
                    pass
                for target in MIDI_TARGETS:
                    disconnect(keylab, target, environment)
                if not link_present(links(environment), keylab, ROUTER_INPUT):
                    run(["pw-link", keylab, ROUTER_INPUT], environment)
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
                    diagnostic_event(
                        diagnostic_log,
                        "transition-start",
                        from_profile=current,
                        requested_profile=profile,
                    )
                    try:
                        start_candidate(profile)
                        diagnostic_event(
                            diagnostic_log,
                            "transition-pass",
                            profile=profile,
                        )
                    except (OSError, RuntimeError) as failure:
                        warm_genre_transition = fast_genre_enabled and (
                            profile in warmed_genres or current in warmed_genres
                        )
                        if warm_genre_transition:
                            fast_genre_enabled = False
                            stop_warm_genres()
                            diagnostic_event(
                                diagnostic_log,
                                "fast-mode-fallback",
                                error=str(failure),
                                feature="genre-prewarming",
                            )
                        elif fast_mode_enabled:
                            fast_mode_enabled = False
                            stop_warm_engines()
                            diagnostic_event(diagnostic_log, "fast-mode-fallback", error=str(failure))
                        transition_failures.append({
                            "requested_profile": profile,
                            "error": str(failure),
                        })
                        print(json.dumps({
                            "requested_profile": profile,
                            "error": str(failure),
                            "status": "transition-fail-recovering",
                        }), flush=True)
                        diagnostic_event(
                            diagnostic_log,
                            "transition-fail",
                            requested_profile=profile,
                            error=str(failure),
                            recovery="started",
                        )
                        try:
                            restore_live()
                            diagnostic_event(
                                diagnostic_log,
                                "transition-recovered",
                                requested_profile=profile,
                                final_profile=current,
                            )
                        except (OSError, RuntimeError) as recovery_failure:
                            transition_failures.append({
                                "requested_profile": "full-live-rack",
                                "error": str(recovery_failure),
                            })
                            diagnostic_event(
                                diagnostic_log,
                                "recovery-fail",
                                requested_profile=profile,
                                error=str(recovery_failure),
                            )
                            stop_requested = True
            time.sleep(0.02)
        restore_live()
    except KeyboardInterrupt:
        error = None
    except (OSError, RuntimeError) as failure:
        error = str(failure)
        diagnostic_event(diagnostic_log, "session-error", error=error, profile=current)
    finally:
        stop_warm_engines()
        stop_warm_genres()
        full_active = run(
            ["systemctl", "--user", "is-active", FULL_CARLA_SERVICE], environment
        ).returncode == 0
        if audio_touched and not full_active:
            try:
                stop_candidate()
                wait_for_carla(False, environment)
                ensure_full_audio()
            except (OSError, RuntimeError) as failure:
                if error is None:
                    error = str(failure)
        if genre_quantum_changed:
            set_pipewire_quantum(DEFAULT_PIPEWIRE_QUANTUM, environment)
            genre_quantum_changed = False
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
                try:
                    connect(keylab, target, environment)
                except RuntimeError as failure:
                    if error is None:
                        error = str(failure)
        for target in MIDI_TARGETS:
            disconnect("s2-arturia-profile-router:out", target, environment)
        if audio_touched:
            for source, target in ARTURIA_AUDIO:
                try:
                    connect(source, target, environment)
                except RuntimeError as failure:
                    if error is None:
                        error = str(failure)

    after = links(environment) if before else ""
    normalized_before = normalized_links(before)
    normalized_after = normalized_links(after)
    result = {
        "profiles_seen": profiles_seen,
        "transition_failures": transition_failures,
        "final_profile": current,
        "duration_ms": arguments.duration_ms,
        "error": error,
        "graph_restored": bool(before) and normalized_before == normalized_after,
        "protected_graph_changed": bool(before) and normalized_before != normalized_after,
        "before_links_sha256": hashlib.sha256(normalized_before.encode()).hexdigest() if before else None,
        "after_links_sha256": hashlib.sha256(normalized_after.encode()).hexdigest() if after else None,
        "schema": "music-studies/s2-arturia-pad-switch/v1",
        "status": "pad-switch-window-pass" if error is None and not transition_failures and normalized_before == normalized_after else "pad-switch-window-fail",
    }
    diagnostic_event(
        diagnostic_log,
        "session-end",
        status=result["status"],
        error=error,
        final_profile=current,
        transition_failures=transition_failures,
    )
    arguments.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["status"] == "pad-switch-window-pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
