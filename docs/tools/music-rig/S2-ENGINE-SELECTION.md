# S2 Engine Selection

Status: candidate decisions selected; isolated package, Carla discovery, and
control-surface inventory checks pass. State, CPU, and live-host binding remain
open.

The current Hammond and Optik entries are SoundFont players. They remain useful
for the protected `full-live-rack`, but they cannot provide the parameterized
organ and synthesizer layouts requested by S2.

## Organ: setBfree

Selected primary candidate: [setBfree](https://github.com/pantherb/setBfree).

Why it fits:

- genuine tonewheel organ model rather than a static organ sample;
- drawbars, percussion, key click, chorus/vibrato, overdrive, reverb, and Leslie behavior;
- MIDI-controlled operation suitable for Arturia faders and knobs;
- LV2 and standalone Linux forms for Carla/Echora evaluation;
- GPL-2.0-or-later licensing and an established Linux audio ecosystem.

Integration decision:

- Treat organ controls as a MIDI-CC control contract first.
- Do not depend on host automation metadata for drawbars; setBfree's control
  surface is primarily MIDI/configuration driven.
- Verify CC assignments, drawbar direction, preset persistence, Leslie state,
  and two-way feedback behavior before binding the profile.

Local staging result:

- Ubuntu package `setbfree 0.8.12+ds-2build2` was unpacked under `/tmp` only.
- Package SHA-256: `6ad090237b6fea0f777db13a52e044d91143df3496e5c7b1b4eccf479c4ba554`.
- Carla discovery reports one MIDI input, one MIDI output, two audio outputs,
  and zero host parameter inputs. This confirms the MIDI-CC integration boundary.
- The package is not installed system-wide or copied to Airstar.
- The reproducible inventory records the discovery and drawbar contract in
  [s2-engine-inventory-2026-09-06.json](benchmarks/s2-engine-inventory-2026-09-06.json).

## Synth: synthv1 Preferred Fallback

Preferred S2 fallback candidate: `synthv1 0.9.34`.

Why it fits:

- ordinary LV2 ControlPorts rather than only LV2 atom parameter metadata;
- two DCOs, filters, envelopes, LFOs, modulation, effects, and output controls;
- 145 host-visible parameter inputs with stable symbols and ranges;
- LV2 state interface and a small Linux package suitable for isolated staging.

Integration decision:

- Evaluate the LV2 ControlPort build first in Carla and the isolated host.
- Bind semantic targets to ordinary symbols such as `DCF1_CUTOFF`,
  `DCF1_RESO`, `DCO1_BALANCE`, `DCA1_ATTACK`, `DCA1_RELEASE`, and
  `OUT1_VOLUME` only after runtime state and MIDI response pass.
- Keep Surge XT as a parked metadata reference; its Carla parameters are
  read-only for ordinary automation in the verified package.

Local staging result:

- Ubuntu package `synthv1-lv2 0.9.34-1build3` was unpacked under `/tmp` only.
- Package SHA-256:
  `4cdadbe09bfea5e9f3b8da53ca619a1478bf5467258a742e2fce52f5247a2e1d`.
- Carla discovery reports two audio inputs, two audio outputs, one MIDI input,
  and 145 parameter inputs. The LV2 TTL exposes ordinary ControlPorts and a
  state interface.
- The package is not installed system-wide or copied to Airstar. Evidence is
  [recorded here](benchmarks/s2-synth-fallback-2026-09-07.json).
- A temporary synthv1 preset archive also loads through the standalone JACK
  client without an error. LV2 state save/restore still fails in the direct
  probe, so the native preset file is accepted as the prepared-state contract;
  LV2 state saving remains unsupported for live promotion.
- The candidate-only `synth-programmer-synthv1` Device/Rig Profile now records
  these symbols and dependencies without being included in `airstar-current`.

Surge XT remains a parked alternative. Its `1.3.4` package exposed 775 LV2
parameter metadata entries, but Carla reported the relevant parameters
read-only and the host probes received no patch responses.

## Verification Gate

Neither candidate is activated or substituted into the protected project. Before
either profile becomes live-capable, the candidate must pass:

1. license and package provenance review;
2. isolated Carla load with no live graph;
3. parameter and MIDI-control inventory with stable names and ranges;
4. preset/state save and restore;
5. offline audio, CPU, and finite-sample checks;
6. prepared-engine resource and rollback tests; and
7. a separately staged operator rehearsal with the protected layout untouched.

The current isolated result closes only the discovery/control-surface portion of
the gate. setBfree is verified as a MIDI-CC engine with inverse nine-position
drawbars and no host parameter inputs. synthv1 is verified with 145 ordinary
LV2 ControlPorts and a state interface. Neither result proves preset
restoration, CPU behavior, semantic MIDI response, or musical response.

The verification host has no system-installed JACK server, so a temporary
uninstalled JACK dummy backend was staged under `/tmp`. Both candidates loaded
and processed idle audio for five seconds at 48 kHz/1024 frames without touching
PipeWire or the protected Carla graph. State restoration, MIDI-control response,
and musical-load CPU validation remain open.

The current `tonewheel-organ` and `synth-programmer` profiles remain semantic
contracts until this gate passes. `full-live-rack` remains the production and
recovery profile. The complete prepared-only target disposition is recorded in
[s2-engine-control-contracts-2026-09-07.json](benchmarks/s2-engine-control-contracts-2026-09-07.json): verified controls have backend identifiers, while unavailable controls are explicit and cannot be activated accidentally.
The prepared-engine transaction boundary and remaining resource-adapter work
are defined in [PREPARED-ENGINE-TRANSACTION.md](PREPARED-ENGINE-TRANSACTION.md).
