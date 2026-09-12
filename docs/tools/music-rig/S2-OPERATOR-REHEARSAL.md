# S2 Operator Rehearsal

Status: offline rehearsal passed; live operator session remains separately gated.

The offline rehearsal covers the operator sequence without touching the
protected rig:

1. Compile the tonewheel-organ layout.
2. Compile the existing synth-programmer semantic layout.
3. Compile the candidate `synth-programmer-synthv1` binding in a temporary
   catalogue.
4. Run output-suppressed dry-runs.
5. Commit the candidate temporarily and switch back to `full-live-rack`.
6. Stage and launch the native synthv1 preset process under dummy JACK, then
   clean up the process and temporary resources.

Evidence is [recorded here](benchmarks/s2-operator-rehearsal-2026-09-07.json).

The following are deliberately not claimed by this rehearsal:

- physical Arturia or other controller response;
- live PipeWire or Carla graph activation;
- audible musical acceptance;
- LV2 state save/restore;
- production promotion or Airstar mutation.

The live rehearsal requires a separate review of the preset state strategy,
operator rollback procedure, and audio acceptance criteria.
