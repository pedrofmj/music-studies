# S2 Live Operator Rehearsal

Status: live MIDI window passed; human controller assessment remains pending.

Approval record: [S2-LIVE-OPERATOR-APPROVAL.md](S2-LIVE-OPERATOR-APPROVAL.md).

The read-only preflight passed on 2026-09-12. The approved Ubuntu synthv1
candidate was extracted under `/tmp` on `airstar` without system installation.
The guarded live transaction connected the KeyLab MIDI input and candidate stereo
output for a short synthetic-MIDI test, then restored the exact original graph.
No protected project or service was changed.

The subsequent 30-second interactive window observed 203 KeyLab MIDI messages
and note events, restored the exact graph, and passed the protected `30/30`
post-check. No CC messages were observed, so fader/knob response is not claimed.
See `benchmarks/s2-live-human-window-2026-09-12.json`.

A corrected isolated window muted the existing live-rack audio upstream of the
Arturia volume encoder while the candidate used the normal speaker destination.
It observed 236 note messages, no CC messages, restored the exact graph, and
passed the protected `30/30` post-check. See
`benchmarks/s2-live-isolated-control-window-2026-09-12.json`.

The focused control window then observed 1,327 CC messages with the live-rack
audio muted. Fader 2 produced CC 75 across the full `0..127` range; knobs 1-6
also produced their recorded CC values. The graph was restored exactly and the
protected `30/30` post-check passed. See
`benchmarks/s2-live-control-mapping-2026-09-12.json`.

The combined musical/control window then observed 299 note messages and 654 CC
messages while the live-rack audio remained muted. CC 75 again traversed the
full `0..127` range, the graph was restored exactly, and the protected `30/30`
post-check passed. See
`benchmarks/s2-live-combined-window-2026-09-12.json`.

The candidate now includes an explicit temporary synthv1 controller map at
`benchmarks/s2-synthv1-keylab-controls.conf`. It maps the verified KeyLab CCs
to native synthv1 parameters through the candidate-only `synthv1.conf` runtime
configuration; the protected rack does not consume this map.

The final two-minute mapped rehearsal observed 449 note messages and 1,618
control changes. The operator confirmed that the controls audibly affected the
synth. The graph restored exactly and the protected `30/30` post-check passed.
See `benchmarks/s2-live-mapped-two-minute-2026-09-12.json`.

## Arturia Profile Choices

The Arturia slot now has three explicit choices:

- `full-live-rack`: current default and rollback target; protected live Carla instruments.
- `synth-programmer-synthv1`: mapped synthv1 candidate.
- `tonewheel-organ-setbfree`: mapped setBfree organ candidate.

The candidate-only runner is:

```bash
python3 docs/tools/music-rig/benchmarks/run-s2-arturia-profile.py \
  --profile synth-programmer-synthv1 \
  --acknowledge-live-routing \
  --synthv1 /tmp/music-rig-synthv1-ubuntu-20260912/root/usr/bin/synthv1_jack \
  --synthv1-preset /tmp/music-rig-synthv1-ubuntu-20260912/50_SynthRemember-ubuntu.synthv1 \
  --synthv1-controls /tmp/music-rig-synthv1-ubuntu-20260912/s2-synthv1-keylab-controls.conf \
  --duration-ms 5000 \
  --output /tmp/s2-profile.json
```

Use the same runner with `--profile tonewheel-organ-setbfree`,
`--setbfree /tmp/music-rig-synthv1-ubuntu-20260912/setbfree-root/usr/bin/setBfree`,
`--setbfree-config /tmp/music-rig-synthv1-ubuntu-20260912/s2-setbfree-keylab-controls.cfg`,
and `--setbfree-library /tmp/music-rig-synthv1-ubuntu-20260912/setbfree-root/usr/lib`.
The runner refuses `full-live-rack`; returning to
the default is the cleanup/rollback state. The ordinary `music-rig` production
transport remains fail-closed until a separately approved persistent engine
ownership transaction is implemented.

The first two-minute `tonewheel-organ-setbfree` window passed its reversible
candidate transaction and protected post-check. The operator confirmed that
organ controls audibly responded. Assessment is recorded separately in
`benchmarks/s2-setbfree-two-minute-window-2026-09-12.json`.

This session is separate from production. It must use one staged candidate
engine and its temporary JACK/PipeWire ports. It must not open or mutate the
protected `full-live-rack` project.

Current approved scope is Arturia KeyLab only. SMK-25 support is deferred to a
later profile. The requested live-rack output is the target destination, but no
temporary candidate-to-live-rack routing transaction is currently authorized.

## Preconditions

- Explicit approval names the operator, date, candidate profile, and abort owner.
- Protected baseline verification passes `30/30` before staging.
- Carla is closed for the protected project and its source/checksum remain unchanged.
- The staged synthv1 archive and preset hashes match the recorded evidence.
- A separate temporary JACK/Carla project and MIDI endpoint are ready.
- The rollback target is `full-live-rack`; no production promotion is authorized.

## Session Steps

1. Record the protected baseline and capture the temporary candidate graph hash.
2. Start the selected candidate engine with its verified configuration/preset.
3. Connect only the temporary candidate MIDI/audio ports.
4. Exercise the selected Arturia control layout: synth faders/knobs or organ
   drawbars, rotary, percussion, swell, drive, and reverb.
5. Confirm parameter response at minimum, center, and maximum values, including
   pickup behavior and no unexpected MIDI feedback.
6. Recall the candidate preset/configuration and confirm the authored
   registration returns.
7. Perform a short musical phrase and record operator audio assessment. The
   automated transaction used synthetic MIDI only; no human control assessment
   is claimed yet.
8. Trigger candidate rollback and confirm candidate process termination, graph
   cleanup, and protected `full-live-rack` recovery.
9. Run the protected verifier again and retain raw logs beside the evidence.

## Abort Conditions

- Any unexpected protected graph link, service change, or production artifact mutation.
- Candidate process crash, unbounded CPU, nonfinite audio, MIDI flood, or stuck control.
- Preset recall or rollback does not restore the pre-session candidate state.
- Audible instability, dropout, xrun, or unexplained control response.

On abort: stop MIDI input, mute/stop the candidate, remove only temporary links,
restore the protected graph from the approved recovery path, and rerun the
protected verifier. Do not promote the candidate.

Evidence template: [s2-live-operator-rehearsal-2026-09-07.json](benchmarks/s2-live-operator-rehearsal-2026-09-07.json).

The candidate-only boundary is available for review: it requires explicit
acknowledgement and routes only to temporary JACK dummy ports. It cannot select
physical endpoints, PipeWire, or `airstar-current`.
