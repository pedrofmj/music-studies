# Prepared Engine Transaction Boundary

Status: caller-owned transaction primitive, output-enabled runtime wiring, and
temporary package staging implemented; plugin-resource activation is not
implemented.

The S2 layouts now have explicit engine-control dispositions, but those
dispositions are not runtime plugin bindings. The protected runtime may compile,
prepare, commit, and switch back an output-suppressed definition; it must not
load setBfree, Surge XT, or mutate the Carla/PipeWire graph from that path.

## Required Resource Lifecycle

The caller-owned transaction primitive exposes these phases:

1. `stage`: resolve a verified package, plugin format, asset set, and immutable
   control metadata in an isolated location.
2. `validate`: check plugin identity, parameter/control coverage, state format,
   stable ports, CPU budget, and finite audio without changing the active rig.
3. `arm`: make the complete candidate resource set available to the runtime
   transaction without publishing it or connecting live graph links.
4. `commit`: publish the prepared generation and confirm resource adoption.
5. `rollback`: restore the previous generation and discard the candidate when
   preparation, confirmation, persistence, or adoption fails.

## Current Coverage

- S2 compiler checks cover every semantic organ and synth target with either a
  backend identifier or an explicit unavailable status.
- Output-suppressed runtime tests commit both S2 layouts, persist temporary
  state, and switch back to `full-live-rack`.
- Existing runtime tests inject output-confirmation and persistence failures,
  verify rollback generations, and reject rollback failures.
- `music_rig_prepared_engine_transaction` adds isolated stage/validate/arm/
  commit/rollback/discard failure injection without loading a plugin.
- Output-enabled global and device commits invoke the transaction and fail closed
  through the existing generation/output rollback path.
- The isolated resource-stage check verifies package hashes, required plugin and
  state assets, and temporary extraction for both candidates; it never installs
  or activates them.
- The synthv1 fallback direct probe changes `DCO1_BALANCE` from `0.000` to
  `0.750` with finite audio; its state save returns success but restore returns
  non-success with no stored properties.
- A temporary native synthv1 preset loads at standalone JACK startup without an
  error; this does not close the LV2 runtime state gate.
- The native preset resource adapter now verifies the archive, XML parameter
  count, candidate symbols, and temporary staging without installation.
- The synthv1 prepared contract selects that native preset file as its candidate
  state resource and validates the candidate-only commit/rollback sequence back
  to `full-live-rack` without entering the Airstar binding.
- The temporary `synthv1-candidate` binding now runs through the real
  output-suppressed daemon commit/switch-back path with durable temporary state.
- Native `.synthv1` files are accepted as the prepared-state contract for this
  candidate; the candidate remains prepared-only and is not a live platform
  binding.
- `music_rig_synthv1_preset_adapter` now implements the native preset file as a
  concrete stage/validate/arm/commit/rollback/discard resource callback.
- The POSIX synthv1 process controller supplies real start/health/stop hooks;
  invalid launch failure is fail-closed and owned processes are terminated on
  rollback/discard.
- An output-enabled runtime test has now started the staged synthv1 process on
  temporary JACK, injected output-confirmation failure, and verified process
  termination plus generation rollback.
- A separate five-second synthv1 musical-load rehearsal delivers note/CC MIDI
  and finite audio at 48 kHz/1024; the wrapper's CPU timing is not attributable
  to the child process and remains diagnostic only.
- The staged native preset has also been loaded by the standalone synthv1 JACK
  process, owned for the temporary session, and cleaned up after termination.
- Both LV2 candidates now load and process idle audio for five seconds through a
  temporary JACK dummy server at 48 kHz/1024 frames; the protected graph is not
  touched.
- Both candidates also remain alive while receiving isolated MIDI note/CC
  stimulus for five seconds; no MIDI feedback was emitted by either LV2 path.
- Surge XT preset enumeration/load was exercised, but Jalv cannot expose its
  LV2 atom parameters through ordinary `set` control commands. setBfree remains
  MIDI-program based. Save/restore is therefore still not runtime-tested.
- A dedicated offline LV2 host now sends a real `patch:Set`/`patch:Get` for
  `a_filter1_cutoff` and verifies finite audio. Surge emitted no patch response,
  so semantic parameter acknowledgement remains unproven.
- The same host calls Surge's LV2 state interface: save returns success but
  stores no properties, and restore returns `LV2_STATE_ERR_NO_PROPERTY`. State
  persistence is therefore not accepted.
- A temporary Carla project loads the staged Surge LV2 plugin on the dummy JACK
  server and registers its OSC backend, but the comparison listener did not
  receive Carla's callback stream. Carla parameter/state comparison remains
  pending; no protected project was opened.
- Carla's startup callback stream nevertheless identifies `A Filter 1 Cutoff`
  as parameter index `25` with range `0..1`, but reports hints `4144` and the
  value remains `0` after OSC `set_parameter_value`; Carla exposes this LV2
  atom property as read-only through its ordinary parameter API.
- No plugin resource is staged into the runtime adapter, and no live graph is
  changed by these tests.

The next implementation slice is runtime ownership of the staged resource and
state restore plus semantic MIDI-control response validation under the isolated JACK host.
The idle CPU readings are diagnostic only, and activation must remain disabled
until the remaining gates pass.
