# Current Rack Startup And Validation Transcript

Status: **PLANNED - NOT EXECUTED**

Rig: `pedro-performance-rig`

Rig Profile: `full-live-rack`

Reference host: `airstar`

This deterministic transcript is generated from
[current-rack-startup.json](current-rack-startup.json) and checked
against the protected setup manifest. Generating or checking this document
does not run any listed command.

## Safety Lock

- The protected single rig remains the production default.
- Only explicitly classified read-only checks may be automated.
- Controller connection, Carla launch, and musical checks are operator-only.
- No failure triggers an automatic restore.
- Restore remains preview-only unless the operator explicitly uses `--apply`
  with Carla stopped and a timestamped backup.

## Expected Authority

| Item | Expected |
| --- | --- |
| Carla plugins | 49 |
| Carla project connections | 111 |
| Deployment graph links | 116 |
| PipeWire quantum | 1024 frames |
| Services | 4 |
| Controllers | 5 |

Expected services:

- `arturia-main-volume-encoder.service`
- `smk25-pad-layers.service`
- `pipewire-carla-quantum.service`
- `pipewire-patchbay-watch.service`

Expected controller models:

- KeyLab Essential 61 mk3
- M-VAVE SMK-25
- M-VAVE SMC-Mixer
- M-VAVE SMC-PAD
- M-VAVE SMC-PAD Pocket

## Execution Record

| Field | Recorded value |
| --- | --- |
| Operator | NOT RECORDED |
| Started at | NOT RECORDED |
| Completed at | NOT RECORDED |
| Hostname | NOT RECORDED |
| Commit | NOT RECORDED |
| Final result | ⬜ NOT RUN |

## Phase 1: Repository Preflight

Execution policy: `may-automate-read-only`

1. **Verify protected source artifacts**

   - Command: `docs/tools/airstar-live-setup/verify-protected-baseline`
   - Mutates live state: no
   - Expected: Protected baseline verified: 30 checks, 0 failures
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

2. **Preview the recovery path**

   - Command: `docs/tools/airstar-live-setup/restore-protected-baseline`
   - Mutates live state: no
   - Expected: Preview only; no restore action is applied
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

## Phase 2: Installed Readiness On Airstar

Execution policy: `operator-read-only`

1. **Validate installed files, assets, and services**

   - Command: `docs/tools/airstar-live-setup/validate-airstar-live-setup --fast`
   - Mutates live state: no
   - Expected: Validation complete: 0 failures
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

## Phase 3: Hardware Connection And Rack Launch

Execution policy: `operator-only`

1. **Connect all five controllers before launching Carla**

   - Operator action: Connect the Arturia, SMK-25, SMC-Mixer, SMC-PAD, and SMC-PAD
                      Pocket and confirm their protected hardware presets.
   - Mutates live state: yes, operator-controlled
   - Expected: One Arturia and four M-VAVE controllers are present
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

2. **Launch the protected Carla rack once**

   - Command: `~/bin/carla-pedro-project`
   - Mutates live state: yes, operator-controlled
   - Expected: Carla opens /c/music/carla/pedro.uproject and finishes loading 49 plugins
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

## Phase 4: Post-Start Read-Only Validation

Execution policy: `operator-read-only`

1. **Validate controllers, Carla, services, quantum, and graph**

   - Command: `docs/tools/airstar-live-setup/validate-airstar-live-setup --live --fast`
   - Mutates live state: no
   - Expected: Validation complete: 0 failures; Patchbay check remains dry-run
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

## Phase 5: Operator Musical Acceptance

Execution policy: `operator-only`

1. **Verify Arturia rack controls**

   - Operator action: Play keys, move the central CC114 encoder, and press/release CC115
                      once; confirm volume tracking and click-free stereo mute.
   - Mutates live state: yes, operator-controlled
   - Expected: Instruments play; rack volume and mute respond without an audible click
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

2. **Verify SMK-25 layers**

   - Operator action: Toggle one Side-A layer, play and release a chord, then use Stop
                      and Play.
   - Mutates live state: yes, operator-controlled
   - Expected: Layer latch, chord replacement, pause, and resume match the protected
               behavior
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

3. **Verify SMC-Mixer equalizer**

   - Operator action: Move faders 1 through 8 one at a time.
   - Mutates live state: yes, operator-controlled
   - Expected: Only the corresponding protected equalizer band changes
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

4. **Verify SMC-PAD drums**

   - Operator action: Play representative kick, snare, and cymbal pads.
   - Mutates live state: yes, operator-controlled
   - Expected: The protected drum instrument responds at the expected level
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

5. **Verify SMC-PAD Pocket drums**

   - Operator action: Play representative kick, snare, and cymbal pads.
   - Mutates live state: yes, operator-controlled
   - Expected: The protected drum instrument responds independently at the expected
               level
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

6. **Verify audible stability**

   - Operator action: Play the rack normally for at least two minutes while exercising
                      the five controllers.
   - Mutates live state: yes, operator-controlled
   - Expected: No dropout, stuck note, unintended route, or audible click is observed
   - Result: ⬜ NOT RUN
   - Evidence: NOT RECORDED

## Failure Policy

On any failed or ambiguous check:

1. Stop new experimental work and preserve this transcript.
2. Do not attempt an automatic graph repair or restore.
3. Preview recovery with `docs/tools/airstar-live-setup/restore-protected-baseline`.
4. Apply recovery only after every recorded prerequisite is satisfied.

Restore apply prerequisites:

- explicit operator approval
- Carla stopped
- timestamped backup
- passing protected-source verification

Explicit restore command: `docs/tools/airstar-live-setup/restore-protected-baseline --apply`

## Evidence Status

The procedure and its safety contract are ready. Live startup evidence has
not been captured. After an operator-controlled run, retain a dated copy
under `docs/tools/music-rig/benchmarks/` and update the feature tracker
only from the recorded results.

Generated by `docs/tools/music-rig/generate-current-rack-startup-transcript`.
