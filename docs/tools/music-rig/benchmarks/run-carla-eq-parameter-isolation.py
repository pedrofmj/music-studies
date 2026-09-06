#!/usr/bin/env python3
"""Measure a copied EQ through Carla's in-process API, without an audio server."""

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import xml.etree.ElementTree as ET


EQ_URI = "http://lsp-plug.in/plugins/lv2/para_equalizer_x8_stereo"
HEADERS = ("CarlaDefines.h", "CarlaNative.h", "CarlaBackend.h", "CarlaHost.h")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check_host_log(log):
    lowered = log.lower()
    for failure in ("assertion failure", "assertion failed", "segmentation fault",
                    "addresssanitizer", "runtime error:", "error opening file"):
        if failure in lowered:
            raise ValueError(f"native host log contains {failure}; reject this run")
    return {"assertion_lines": 0,
            "known_filter_select_warnings": log.count("Broken plugin parameter 'Filter select'"),
            "hidden_ui_update_messages": log.count("update while UI is hidden")}


def profile_quality(summary):
    warnings = [line.strip() for line in summary.splitlines() if "warning:" in line.lower()]
    return {"warnings": warnings, "quantitative_profile_valid": not warnings,
            "interpretation": "qualitative stacks only" if warnings else "sampling estimates"}


def profile_command(target, output):
    return ["gprofng", "collect", "app", "-o", str(output / "profile.er"),
            "-p", "on", "-F", "off", "-a", "usedldobjects"] + target


def isolated_project(source):
    root = ET.fromstring(source)
    plugins = [p for p in root.findall("Plugin") if p.findtext("Info/URI") == EQ_URI]
    if len(plugins) != 1:
        raise ValueError("source must contain exactly one LSP stereo 8-band EQ")
    plugin = copy.deepcopy(plugins[0])
    if plugin.findtext("Info/Type") != "LV2" or plugin.findtext("Data/Active") != "Yes":
        raise ValueError("EQ must be an active LV2 plugin")
    if plugin.findtext("Data/Options") != "0x1":
        raise ValueError("unexpected EQ options; review the fixed-buffer test contract")
    parameters = plugin.findall("Data/Parameter")
    for band in range(8):
        gains = [p for p in parameters if p.findtext("Symbol") == f"g_{band}"]
        if len(gains) != 1:
            raise ValueError(f"missing or duplicate gain parameter g_{band}")
        gain = gains[0]
        if gain.findtext("MidiChannel") != "1" or gain.findtext("MappedControlIndex") != str(102 + band):
            raise ValueError(f"unexpected MIDI mapping for g_{band}")
        minimum = float(gain.findtext("MappedMinimum", "nan"))
        maximum = float(gain.findtext("MappedMaximum", "nan"))
        if not 0 < minimum < 1 < maximum < 16:
            raise ValueError(f"invalid gain mapping range for g_{band}")
    isolated = ET.Element("CARLA-PROJECT", {"VERSION": "2.0"})
    isolated.append(plugin)
    ET.indent(isolated)
    return ET.tostring(isolated, encoding="utf-8", xml_declaration=True)


def run(args):
    started = datetime.now(timezone.utc).isoformat()
    source = args.project.resolve(strict=True)
    project_bytes = isolated_project(source.read_bytes())
    includes = args.carla_include.resolve(strict=True)
    library = args.carla_library.resolve(strict=True)
    bundle = args.lv2_bundle.resolve(strict=True)
    symbols = args.profile_symbols.resolve(strict=True) if args.profile_symbols else None
    for header in HEADERS:
        (includes / header).resolve(strict=True)
    for filename in ("manifest.ttl", "para_equalizer_x8_stereo.ttl", "lsp-plugins-lv2.so"):
        (bundle / filename).resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    project = output / "eq-only.carxp"
    project.write_bytes(project_bytes)
    binary = output / "carla-eq-parameter-isolation"
    code = Path(__file__).with_name("carla-eq-parameter-isolation.c")
    source_hashes = {"harness_source": sha256(code), "runner_source": sha256(__file__)}
    command = [args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I", str(includes), str(code), str(library),
               f"-Wl,-rpath,{library.parent}", "-lm", "-o", str(binary)]
    if args.profile:
        command[1:1] = ["-g", "-fno-omit-frame-pointer"]
    with (output / "build.log").open("w") as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=60)
    raw_csv = output / "blocks.csv"
    raw_json = output / "measurement.json"
    with tempfile.TemporaryDirectory(prefix="eq-isolation-home-") as home:
        # Discovery sees only the explicitly selected bundle, not sibling evidence directories.
        lv2_path = Path(home) / "lv2"
        lv2_path.mkdir()
        (lv2_path / bundle.name).symlink_to(bundle, target_is_directory=True)
        env = os.environ.copy()
        env.update(HOME=home, XDG_CONFIG_HOME=home, XDG_CACHE_HOME=home,
                   LV2_PATH=str(lv2_path), PIPEWIRE_REMOTE="eq-isolation-no-server",
                   JACK_NO_START_SERVER="1", JACK_DEFAULT_SERVER="eq-isolation-no-server")
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        target = [str(binary), str(project), str(lv2_path),
                  str(library.parent / "resources"), str(args.blocks), str(raw_csv), str(raw_json)]
        if args.profile == "gprofng":
            target = profile_command(target, output)
        with (output / "host.log").open("w") as log:
            subprocess.run(target,
                           check=True, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    measurement = json.loads(raw_json.read_text())
    health = check_host_log((output / "host.log").read_text())
    if len(measurement["scenarios"]) != 15:
        raise ValueError("incomplete scenario matrix")
    profiling = None
    if args.profile == "gprofng":
        display = ["gprofng", "display", "text"]
        if symbols:
            display += ["-addpath", str(symbols)]
        display += ["-header", "-statistics", "-limit", "30", "-functions",
                    "-limit", "50", "-calltree", str(output / "profile.er")]
        summary_path = output / "profile-summary.txt"
        with summary_path.open("w") as summary:
            subprocess.run(display, stdout=summary, stderr=subprocess.STDOUT, check=True, timeout=60)
        profiling = {"collector": args.profile, "kernel_counters_used": False,
                     "profile_symbols": str(symbols) if symbols else None,
                     "summary_sha256": sha256(summary_path),
                     **profile_quality(summary_path.read_text())}
    evidence = {
        "schema": "music-studies/carla-eq-parameter-isolation/v1",
        "captured_at_utc": {"start": started, "end": datetime.now(timezone.utc).isoformat()},
        "environment": {"host": platform.node(), "platform": platform.platform(),
                        "machine": platform.machine(), "carla_library": str(library),
                        "lv2_bundle": str(bundle), "audio_server_used": False,
                        "hardware_connections": False, "plugin_ui_opened": False,
                        "plugin_offline_hint": False},
        "source_project": {"path": str(source), "sha256": sha256(source)},
        "sha256": {"carla_library": sha256(library),
                   "lsp_binary": sha256(bundle / "lsp-plugins-lv2.so"),
                   "lsp_metadata": sha256(bundle / "para_equalizer_x8_stereo.ttl"),
                   "isolated_project": sha256(project), **source_hashes,
                   "harness_binary": sha256(binary),
                   "headers": {name: sha256(includes / name) for name in HEADERS},
                   "blocks_csv": sha256(raw_csv)},
        "measurement": measurement,
        "host_log_health": health,
        "profiling": profiling,
        "limitations": [
            "This is the EQ slice in Carla's native rack API, not the full live multiple-client graph.",
            "CC102-109 enter the EQ directly; the upstream CC40-47 scaling intermediaries are excluded.",
            "The deterministic stereo tones and noise are not a replay of Arturia audio.",
            "Blocks run as fast as possible on the calling thread, without realtime scheduling or device deadlines.",
            "Elapsed-time over-quantum blocks are diagnostic timings, not measured JACK or PipeWire xruns.",
            "Wall and calling-thread CPU timings include direct setter calls in the direct-update scenario.",
            "The plugin uses its normal realtime mode, with no UI; GUI and worker-thread costs are not isolated."
        ],
    }
    if profiling:
        evidence["limitations"].append(
            "Profiling includes initialization, calibration, warmup, measurement, and cleanup on all threads; "
            "sampling overhead can change timings. Collector warnings invalidate precise percentage claims.")
    (output / "report.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(output / "report.json")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--carla-include", type=Path, required=True)
    parser.add_argument("--carla-library", type=Path, required=True)
    parser.add_argument("--lv2-bundle", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="new evidence directory")
    parser.add_argument("--blocks", type=int, default=2048)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--profile", choices=("gprofng",),
                        help="optional user-space clock sampling; no kernel counters")
    parser.add_argument("--profile-symbols", type=Path,
                        help="directory with matching separate debug symbols for profile reporting")
    args = parser.parse_args()
    if not 128 <= args.blocks <= 100000:
        parser.error("--blocks must be between 128 and 100000")
    if args.profile_symbols and not args.profile:
        parser.error("--profile-symbols requires --profile")
    run(args)


if __name__ == "__main__":
    main()
