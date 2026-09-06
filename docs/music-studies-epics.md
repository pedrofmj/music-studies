# Music Studies: Epic Map

This document groups the repository's learning and practice material into
epics. Device-specific notes remain in `src/`; this map defines the outcomes
that connect them.

## Status Key

| Mark | Meaning |
| --- | --- |
| ✅ | Outcome established and documented |
| 🟡 | Active or partially established |
| ⬜ | Planned, not started |
| ⛔ | Blocked by a named dependency |

## Epic Summary

| Epic | Outcome | Status | Main material |
| --- | --- | --- | --- |
| M1. Study System And Evidence | Dated sessions, practice logs, backups, catalogs, and rebuildable records make progress reviewable. | 🟡 | Music-theory collection, instrument learning logs, controller backups |
| M2. Music Theory And Musicianship | Written theory develops from notation and rhythm through intervals, scales, chords, harmony, ear work, and musical function. | 🟡 | `src/music-theory/learning/01-core-music-theory.md` |
| M3. Keyboard Technique And Harmonic Fluency | Scales, fingering, inversions, arpeggios, cadences, accompaniment, and relaxed movement become dependable across keys. | ⬜ | `src/music-theory/learning/02-arpeggios-and-fingering.md` |
| M4. Ear Training And Sound Literacy | The player can identify, compare, describe, and choose keyboard sound families by musical role. | 🟡 | `src/music-theory/learning/03-keyboard-sound-exploration.md`, sound reference |
| M5. XPS-30 Instrument Mastery | The keyboard is understood, cataloged, backed up, organized into favorites, used in performances, and edited intentionally. | 🟡 | XPS-30 roadmap, sound survey, practice log |
| M6. Controller Fluency | Arturia and M-VAVE controllers are understood as musical and control surfaces with documented mappings and recovery procedures. | 🟡 | `src/midi-controllers/` |
| M7. Live Audio And MIDI Craft | Interfaces, mixers, Carla, PipeWire, routing, monitoring, and the performance rig form a reproducible live workflow. | 🟡 | `src/audio-interfaces/`, `src/audio-software/`, Airstar setup |
| M8. Worship Repertoire And Arrangement | Technical study becomes usable music through songs, service roles, arrangements, accompaniment, and recorded practice. | ⬜ | XPS-30 performances, arrangement/rendering work |
| M9. Historical Music, Musicians And Genres | A connected catalog covers rhythms, genres, musicians, authors, bands, traditions, origins, evolution, and musical characteristics from archaeological evidence through the present. | ⬜ | Planned catalog under `src/music-history/` |

## Learning Order

### Sprint L1: Establish The Study Loop

- Choose one primary theory text and one keyboard method.
- Create the dated study log and define what counts as a completed session.
- Complete the first notation, rhythm, interval, and major-scale exercises.
- Record what can be explained, played, heard, and used musically.

### Sprint L2: Build Keyboard Grammar

- Connect each studied key as scale, diatonic chords, inversions, arpeggio, cadence, and progression.
- Record comfortable tempos and fingering decisions.
- Expand deliberately across keys rather than collecting isolated shapes.

### Sprint L3: Connect Sound To Function

- Compare sound families using identical notes, register, rhythm, dynamics, and progression.
- Record attack, sustain, brightness, texture, role, and layering behavior.
- Apply the comparisons to the XPS-30 sound survey and practice log.

### Sprint L4: Make The Rig Reproducible

- Complete controller mapping and backup baselines.
- Document audio-interface, mixer, Carla, PipeWire, and monitoring paths.
- Use the configurable performance-rig epic map for runtime changes; do not mix live-system experiments into ordinary practice work.

### Sprint L5: Apply And Perform

- Build a small set of reliable piano, pad, organ, strings, and layered performances.
- Apply theory and technique to real worship progressions and arrangements.
- Record short practice outcomes and review musical function, timing, sound choice, and physical comfort.

### Sprint L6: Build The Music-History Catalog

- Define catalog schemas for people, groups, genres, works, traditions, rhythms, instruments, places, eras, and sources.
- Separate documented evidence, scholarly interpretation, oral-tradition limits, and uncertain attribution.
- Start with a timeline that reaches from archaeological and early written evidence through medieval, modern, and present-day music.
- Record relationships such as predecessor, evolution, fusion, diaspora, influence, reaction, revival, and parallel development.

### Sprint L7: Study Rhythms, Genres And People

- Map rhythmic characteristics such as meter, beat emphasis, subdivision, syncopation, swing, polyrhythm, tempo, and dance function.
- Describe genre characteristics without reducing a tradition to one stereotype; include instrumentation, melody, harmony, form, texture, timbre, performance practice, and social function.
- Catalog composers, authors, performers, bands, producers, and influential communities with representative works and historical context.
- Compare current musicians with earlier musicians and traditions, recording both continuity and change.
- Include a positive-listening note for every studied entry: what is technically, historically, emotionally, or culturally valuable even when the style is not personally preferred.

## Epic Exit Rules

- A reading task is not complete until its concept is explained and applied.
- A technique task is not complete until it is played slowly, evenly, and without avoidable tension.
- A sound task is not complete until the sound is compared in a controlled musical context.
- A device task is not complete until its state, mapping, backup, and recovery path are documented.
- A live task is not complete until its routing, operator procedure, rollback, and evidence are reproducible.
- A history entry is not complete until its source, date or period, cultural context, musical characteristics, relationships, and positive listening observations are recorded.

## Relationship To The Performance Rig

The [Configurable Performance Rig epic map](features/0001.0000.0000.0000-configurable-performance-rig/configurable-performance-rig-epics.md)
is the technical-system workstream for M7. It should support the musical study
epics, not replace them: runtime safety and low-latency routing are prerequisites
for dependable practice and performance, while musical acceptance remains a
separate human outcome.

M9 is intentionally broader than a list of favorites. It should represent music
that the student likes and music that is unfamiliar or personally unattractive,
with the same respect for evidence and craft. The catalog should make it possible
to ask who made the music, when and where it developed, which rhythms and forms
define it, what earlier traditions shaped it, what it influenced, and what can be
learned from it today.
