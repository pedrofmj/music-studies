# Current Rack Startup Result: 2026-08-10

Status: **PASS**

Rig Profile: `full-live-rack`

Reference host: `airstar`

Operator: `pedro.ferreira`

Source commit: `93d776517259f960aee3da3f8224d34110075746`

Execution window: 2026-08-10T16:24:40-03:00 through
2026-08-10T16:38:34-03:00.

This is the retained result of the reusable
[startup contract](../current-rack-startup.json) and
[planned transcript](../CURRENT-RACK-STARTUP.md).

## Results

| Phase | Result | Evidence |
| --- | --- | --- |
| Repository preflight | PASS | Protected baseline 30/30; restore remained preview-only |
| Installed readiness | PASS | Airstar validation completed with zero failures |
| Hardware and launch | PASS | One Arturia and four M-VAVE devices; presets confirmed; old Carla process stopped; protected launcher loaded 49 plugins |
| Post-start validation | PASS | Four services active; 2048-frame quantum; 117/117 live Patchbay links present; zero would restore |
| Musical acceptance | PASS | Arturia, SMK-25, SMC-Mixer, SMC-PAD, and SMC-PAD Pocket passed |
| Stability | PASS | At least two minutes of normal playing; no reported dropout, stuck note, unintended route, or click |

## Final Observation

- PipeWire error total: 0.
- Service restart count: 0 for all four protected services.
- Missing protected graph links: 0.
- The pre-existing
  `PD-CH-1 Gain Map:events-out -> smc-pad-gain-verify:input` diagnostic
  connection was present after the test and remains outside the protected
  baseline.
- Carla remained running with the protected rack.

## Safety And Provenance

The operator closed the previous Carla process, launched only
`~/bin/carla-pedro-project`, and exercised normal controller controls.
Validation did not change graph links or services, no restore was applied, and
no protected artifact changed.

Because the repository is not checked out on Airstar, installed-state
validation used a temporary two-file bundle. Both files were verified against
their recorded SHA-256 values before execution and the temporary directory was
removed automatically. An earlier installed-state probe against the agent
container was rejected as non-reference-host evidence and changed no Airstar
state.

Machine-readable evidence:
[current-rack-startup-2026-08-10.json](current-rack-startup-2026-08-10.json).
