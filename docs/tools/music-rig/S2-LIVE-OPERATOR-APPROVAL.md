# S2 Live Operator Approval

Status: explicitly approved in the operator conversation; final preflight passed,
creation of a temporary Ubuntu-package candidate was explicitly approved on
2026-09-12T16:42:12-03:00, and the mapped two-minute rehearsal passed.

Read-only preflight: passed on 2026-09-12. The candidate remains prepared-only;
no production promotion is authorized.

PipeWire is active for `pedro.ferreira` on `/run/user/50001`. The initial tool
probe omitted `XDG_RUNTIME_DIR` and therefore reported a false connection
failure; the probe was repeated with the desktop session environment and
completed read-only.

The protected binding identifies the Arturia role as an Arturia KeyLab Essential
61 mk3 with expected PipeWire alias `KL Essential 61 mk3:KL Essential 61 mk3
MIDI`. Read-only inspection on `airstar` confirmed that endpoint, the active
Carla project, and all nine `AR-CH` instrument nodes. The existing final audio
route is `SMC-MIX - 8-Band EQ` to
`alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__sink:playback_FL/FR`
(Speaker + Headphones). No physical link, service, or protected project state
was changed.

The final protected preflight passed `30/30` on `airstar`. The recorded candidate
evidence hashes validate, but the candidate archive/executable is not currently
staged on `airstar` (`synthv1_jack`, `synthv1`, and `synthv1_lv2` are absent).
The operator approved a new temporary candidate from Ubuntu
`synthv1 0.9.34-1build3`; it will be extracted under `/tmp` only, not installed
system-wide. Its hashes, live routing transaction, MIDI stimulus, graph
restoration, and post-session protected verification are recorded in
`benchmarks/s2-synthv1-ubuntu-candidate-2026-09-12.json` and
`benchmarks/s2-live-candidate-bridge-2026-09-12.json`.

## Authorization

- Operator: `Pedro Ferreira`
- Approver: `Pedro Ferreira`
- Abort owner: `Pedro Ferreira`
- Scheduled date/time: `2026-09-12T16:23:55-03:00; operator readiness confirmed in this conversation`
- Candidate: `synth-programmer-synthv1`
- Controller scope: `Arturia KeyLab` only; SMK-25 is deferred.
- Candidate audio destination: existing live-rack output destination; temporary
  candidate-to-live-rack routing explicitly approved in this conversation.
- Rollback target: `full-live-rack`
- Promotion decision: `no production promotion`

## Required Confirmations

- [x] Protected baseline verifier passes immediately before staging.
- [x] Staged package and preset hashes match recorded evidence.
- [x] Temporary JACK/Carla project and MIDI endpoint are identified.
- [x] Candidate-to-live-rack output routing transaction is explicitly approved.
- [x] No protected project, service, PipeWire link, or production preset will be opened or changed.
- [x] Operator can stop MIDI input and candidate process immediately.
- [x] Abort owner is present and owns rollback authority.
- [x] Post-session protected verification passes (`30/30`).

The baseline, staged-resource hash, and post-session verification checks remain
open until each command completes successfully.
