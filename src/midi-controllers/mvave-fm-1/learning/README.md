# FM-1 Learning Path

## Session 1: Baseline, Audio, And USB MIDI

1. Record firmware version, battery state, voice number, algorithm, global MIDI
   channels, and speaker/headphone behavior.
2. Listen to one factory voice through the speaker, then through headphones or
   the intended audio interface.
3. Connect USB-C and record the created MIDI endpoint.
4. Play local keys and send one external MIDI note to confirm both instrument
   and controller behavior.
5. Create a baseline entry in [Backups](../backups/README.md).

## Session 2: Presets And Performance

1. Survey ten factory voices across piano, bass, brass, pad, and electronic
   categories.
2. Record useful voice number, name, algorithm, effect state, and intended use
   in [Mappings](../mappings/README.md).
3. Test octave, transpose reset, mono/poly behavior, pitch bend range, and one
   arpeggiator mode.
4. Build one restrained patch for a musical role and one experimental patch.

## Session 3: FM Architecture

1. Choose a factory voice to duplicate or preserve as a reference.
2. Change algorithm only, then compare its carrier/modulator behavior.
3. Edit one operator envelope, one output level, and one LFO parameter.
4. Test effects only after the dry tone is understood.
5. Save and document the voice only when it can be described and rebuilt.

## Session 4: Sequencing And MIDI

1. Create a short 16-step sequence and save its pattern number.
2. Test ARP and SEQ separately; confirm the required disable-before-enable
   behavior.
3. Test MIDI input from one external controller and document note and channel.
4. Treat SysEx import and firmware changes as separate, backup-first work.

The goal is control over a small number of explainable sounds, not rapid
overwriting of all 128 voice locations.