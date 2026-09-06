#!/usr/bin/env python3
"""Exercise the observer and trace joiner without live MIDI or PipeWire access."""

import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


TOOLS = Path(__file__).resolve().parents[2] / "airstar-live-setup"
START = "2026-09-05T12:00:00Z"
EPOCH = 1788609600
HEADER = "S ID QUANT RATE WAIT BUSY W/Q B/Q ERR FORMAT NAME\n"


def node(node_id, errors, wait="0.0us", busy="0.0us", wait_ratio="0.00", busy_ratio="0.00"):
    return (f"R {node_id} 1024 48000 {wait} {busy} {wait_ratio} {busy_ratio} "
            f"{errors} + fixture-{node_id}\n")


def trace(second, events=4):
    return (
        f"trace-second {second} input-events {events} mapped-events {events} "
        f"emitted-events 1 coalesced-events {events - 1} unmapped-events 0 "
        "malformed-events 0 adapter-failures 0\n"
    )


class CorrelationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="music-rig-correlation-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.scratch = self.root / "scratch"
        self.scratch.mkdir()
        self.environment = dict(
            os.environ, PATH=f"{self.bin}:{os.environ['PATH']}",
            TMPDIR=str(self.scratch),
        )
        self.mock("pw-link", '[ "$*" = "-l" ] || exit 99\nprintf "fixed graph\\n"')
        self.mock("journalctl", "exit 0")
        self.mock("timeout", 'shift 3\n"$@"\nexit 124')
        self.mock("stdbuf", 'shift\nexec "$@"')
        self.mock("date", f"""case "$*" in
            *%s*) printf '%s\\n' '{EPOCH}' ;;
            *%NZ*) printf '%s\\n' '2026-09-05T12:00:00.500000000Z' ;;
            *) printf '%s\\n' '{START}' ;;
        esac""")

    def mock(self, name, body):
        path = self.bin / name
        path.write_text("#!/usr/bin/env bash\nset -eu\n" + body + "\n")
        path.chmod(0o755)

    def fixture(self, name, content):
        path = self.root / name
        path.write_text(content)
        self.mock(name, f'exec cat "{path}"')

    def observe(self, midi="", snapshots=None):
        self.fixture("aseqdump", "Waiting for data.\n" + midi)
        self.fixture("pw-top", snapshots or (
            HEADER + node(100, 5) + HEADER + node(100, 5)
            + HEADER + node(100, 8)
        ))
        result = subprocess.run(
            ["bash", str(TOOLS / "control-xrun-observer"), "2", "20:1,16:0"],
            env=self.environment, capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list(self.scratch.iterdir()), [])
        return json.loads(result.stdout)

    def test_observer_with_mixer_and_note_input(self):
        report = self.observe(
            " 20:1 Control change 0, controller 40, value 80\n"
            " 16:0 Note on 0, note 60, velocity 90\n"
            " 16:0 Note off 0, note 60, velocity 0\n"
        )
        self.assertEqual(report["midi"]["event_count"], 3)
        bucket = report["correlation"]["per_second"][0]["midi"]
        self.assertEqual(bucket["second"], 0)
        self.assertEqual(bucket["count"], 3)
        self.assertEqual(len(bucket["sources"]), 2)
        self.assertTrue(report["protected_graph"]["unchanged"])
        self.assertEqual(report["command_status"]["aseqdump_timeout_status"], 124)

    def test_final_error_sample_is_retained_without_midi(self):
        report = self.observe()
        seconds = report["correlation"]["per_second"]
        self.assertEqual([row["second"] for row in seconds], [0, 1, 2])
        self.assertEqual(seconds[-1]["pipewire"]["nodes"][0]["err_delta"], 3)
        self.assertEqual(report["pipewire"]["pw_top"]["total_err_delta"], 3)
        self.assertEqual(report["midi"]["event_count"], 0)
        self.assertEqual(seconds[-1]["midi"]["control_changes"], [])

    def test_control_changes_keep_source_channel_and_value_range(self):
        report = self.observe(
            " 20:1 Control change 0, controller 40, value 80\n"
            " 20:1 Control change 0, controller 40, value 20\n"
            " 20:1 Control change 0, controller 41, value 100\n"
            " 20:0 Control change 0, controller 40, value 80\n"
            " 16:0 Control change 0, controller 40, value 50\n"
            " 20:1 Control change 1, controller 40, value 10\n"
            " 16:0 Note on 0, note 40, velocity 90\n"
        )
        controls = report["midi"]["control_changes"]
        self.assertEqual(len(controls), 5)
        mixer = next(row for row in controls if
                     (row["source"], row["channel"], row["controller"]) == ("20:1", 0, 40))
        self.assertEqual(mixer, {"source": "20:1", "channel": 0, "controller": 40,
                                "count": 2, "value_first": 80, "value_last": 20,
                                "value_min": 20, "value_max": 80})
        self.assertEqual(report["correlation"]["per_second"][0]["midi"]["control_changes"], controls)
        self.assertEqual(report["midi"]["unparsed_control_change_count"], 0)

    def test_bad_control_payload_is_counted_without_discarding_other_events(self):
        report = self.observe(
            " 20:1 Control change 0, unrecognized payload\n"
            " 20:1 Control change 16, controller 40, value 80\n"
            " 20:1 Control change 0, controller 128, value 80\n"
            " 20:1 Control change 0, controller 40, value 128\n"
            " 20:1 Control change 0, controller 40, value 80\n"
        )
        self.assertEqual(report["midi"]["event_count"], 5)
        self.assertEqual(report["midi"]["unparsed_control_change_count"], 4)
        self.assertEqual(len(report["midi"]["control_changes"]), 1)

    def test_timing_units_and_summary_maxima(self):
        report = self.observe(snapshots=(
            HEADER + node(100, 5, "4.7ms", "22.9us", "0.22", "0.00")
            + HEADER + node(100, 5, "500ns", "1.5ms", "0.00", "0.07")
            + HEADER + node(100, 8, "1.2s", "300us", "56.25", "0.01")
        ))
        seconds = report["correlation"]["per_second"]
        first = seconds[0]["pipewire"]["nodes"][0]["timing"]
        self.assertEqual(first["wait_raw"], "4.7ms")
        self.assertEqual(first["wait_us"], 4700)
        self.assertEqual(first["busy_us"], 22.9)
        self.assertEqual(first["wait_quantum_ratio"], 0.22)
        self.assertEqual(first["quantum_frames"], 1024)
        self.assertEqual(first["rate_hz"], 48000)
        self.assertEqual(seconds[1]["pipewire"]["nodes"][0]["timing"]["wait_us"], 0.5)
        summary = report["pipewire"]["pw_top"]["timing_nodes"][0]
        self.assertEqual(summary["wait_us_max"], 1200000)
        self.assertEqual(summary["busy_us_max"], 1500)
        self.assertEqual(summary["wait_quantum_ratio_max"], 56.25)
        self.assertEqual(summary["busy_quantum_ratio_max"], 0.07)
        self.assertEqual(summary["wait_samples"], 3)

    def test_timing_unknowns_and_follower_quantum_are_preserved(self):
        report = self.observe(snapshots=(
            HEADER + "R 381 0 0 +++ --- +++ ??? 5 + SMC-MIX - 8-Band EQ\n"
            + HEADER + "R 381 0 0 ??? +++ ??? +++ 8 + SMC-MIX - 8-Band EQ\n"
        ))
        timing = report["correlation"]["per_second"][0]["pipewire"]["nodes"][0]["timing"]
        self.assertEqual(timing["wait_raw"], "+++")
        self.assertEqual(timing["busy_raw"], "---")
        self.assertIsNone(timing["wait_us"])
        self.assertIsNone(timing["busy_us"])
        self.assertIsNone(timing["busy_quantum_ratio"])
        self.assertEqual(timing["quantum_frames"], 0)
        self.assertEqual(timing["rate_hz"], 0)
        summary = report["pipewire"]["pw_top"]["timing_nodes"][0]
        self.assertIsNone(summary["wait_us_max"])
        self.assertEqual(summary["wait_samples"], 0)
        self.assertEqual(report["pipewire"]["pw_top"]["total_err_delta"], 3)

    def test_join_preserves_per_control_and_timing_details(self):
        report = self.observe(" 20:1 Control change 0, controller 40, value 80\n")
        result, output = self.join(report, self.relay_log())
        self.assertEqual(result.returncode, 0, result.stderr)
        joined = json.loads(output.read_text())
        row = joined["per_second"][0]
        self.assertEqual(row["midi"]["control_changes"][0]["controller"], 40)
        self.assertEqual(row["pipewire"]["nodes"][0]["timing"]["wait_us"], 0)

    def test_empty_snapshot_and_node_gap_preserve_counter_delta(self):
        report = self.observe(snapshots=(
            HEADER + node(100, 5) + HEADER + HEADER + node(100, 8)
        ))
        seconds = report["correlation"]["per_second"]
        self.assertEqual(seconds[1]["pipewire"]["nodes"], [])
        self.assertEqual(seconds[2]["pipewire"]["nodes"][0]["err_delta"], 3)

    def test_initial_empty_and_malformed_snapshot_rows_keep_sample_positions(self):
        report = self.observe(snapshots=(
            HEADER + HEADER + node(100, 5) + "R truncated row\n"
            + HEADER + node(100, 8)
        ))
        seconds = report["correlation"]["per_second"]
        self.assertEqual(seconds[0]["pipewire"]["nodes"], [])
        self.assertEqual(seconds[1]["pipewire"]["nodes"][0]["err_delta"], 0)
        self.assertEqual(seconds[2]["pipewire"]["nodes"][0]["err_delta"], 3)

    def join(self, observer, relay, output=None):
        source = self.root / "observer.json"
        source.write_text(json.dumps(observer))
        log = self.root / "relay.log"
        log.write_text(relay)
        output = output or self.root / "joined.json"
        result = subprocess.run(
            ["bash", str(TOOLS / "join-relay-control-xrun-correlation"),
             str(source), str(log), str(output)],
            capture_output=True, text=True, timeout=10,
        )
        return result, output

    def relay_log(self):
        return (f"result 0\ntrace-start-epoch {EPOCH - 1}\n"
                "trace-sample-rate 48000\n" + trace(1) + trace(2, 8))

    def test_join_offsets_and_missing_trace_coverage(self):
        report = self.observe()
        result, output = self.join(report, self.relay_log())
        self.assertEqual(result.returncode, 0, result.stderr)
        joined = json.loads(output.read_text())
        seconds = joined["per_second"]
        self.assertEqual([row["relay_second"] for row in seconds], [1, 2, 3])
        self.assertEqual(seconds[0]["relay"]["input_events"], 4)
        self.assertEqual(seconds[1]["relay"]["input_events"], 8)
        self.assertIsNone(seconds[2]["relay"])
        self.assertEqual(seconds[2]["pipewire_error_delta"], 3)
        self.assertEqual(joined["relay"]["matched_seconds"], 2)
        self.assertEqual(joined["observation"]["command_status"], report["command_status"])
        self.assertEqual(joined["observation"]["protected_graph"], report["protected_graph"])

    def test_full_900_second_trace_avoids_shell_argument_limit(self):
        report = self.observe()
        relay = (f"trace-start-epoch {EPOCH}\ntrace-sample-rate 48000\n"
                 + "".join(trace(second) for second in range(900)))
        result, output = self.join(report, relay)
        self.assertEqual(result.returncode, 0, result.stderr)
        joined = json.loads(output.read_text())
        self.assertEqual(joined["relay"]["trace_seconds"], 900)
        self.assertEqual(joined["relay"]["matched_seconds"], 3)

    def test_invalid_relay_logs_preserve_existing_output(self):
        report = self.observe()
        valid = self.relay_log()
        invalid = {
            "missing_epoch": valid.replace(f"trace-start-epoch {EPOCH - 1}\n", ""),
            "missing_rate": valid.replace("trace-sample-rate 48000\n", ""),
            "zero_rate": valid.replace("trace-sample-rate 48000", "trace-sample-rate 0"),
            "malformed_trace": valid + "trace-second broken\n",
            "truncated_trace": valid + "trace-second\n",
            "duplicate_second": valid + trace(1),
            "duplicate_header": valid + f"trace-start-epoch {EPOCH}\n",
            "no_traces": valid.split("trace-second")[0],
        }
        for name, relay in invalid.items():
            with self.subTest(name=name):
                output = self.root / "joined.json"
                output.write_text("existing evidence\n")
                result, _ = self.join(report, relay, output)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(output.read_text(), "existing evidence\n")

    def test_wrong_observer_schema_is_rejected(self):
        report = self.observe()
        report["schema"] = "unrelated/v1"
        result, _ = self.join(report, self.relay_log())
        self.assertNotEqual(result.returncode, 0)

    def test_duplicate_observer_seconds_are_rejected(self):
        report = self.observe()
        report["correlation"]["per_second"].append(report["correlation"]["per_second"][0])
        result, _ = self.join(report, self.relay_log())
        self.assertNotEqual(result.returncode, 0)

    def test_output_cannot_alias_an_input(self):
        report = self.observe()
        for name in ("observer.json", "relay.log"):
            with self.subTest(name=name):
                alias = self.root / "output-alias.json"
                alias.unlink(missing_ok=True)
                alias.symlink_to(self.root / name)
                result, _ = self.join(report, self.relay_log(), alias)
                self.assertNotEqual(result.returncode, 0)
                expected = json.dumps(report) if name == "observer.json" else self.relay_log()
                self.assertEqual(alias.read_text(), expected)


if __name__ == "__main__":
    unittest.main()
