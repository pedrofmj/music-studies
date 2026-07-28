# LMMS Learning Path

Run each session on Linux and Windows independently. Record the actual version,
audio backend, MIDI endpoint names, interface or driver, and result in
reference/ after each test.

## 01. Establish A Baseline

Install LMMS from the official project path for the target OS. Create a blank
project, choose the intended audio output, enable a MIDI input only when one is
present, and save the version and setup evidence.

Success condition: LMMS opens, saves a new project, receives an intentional
test note when a controller is connected, and produces monitored audio.

## 02. Build One Pattern

Create a short pattern with an included instrument, program notes in the piano
roll, add one basic volume change, and save the project as the first
cross-platform test artifact.

Success condition: the pattern is audible, persists after reopening, and has a
recorded project filename.

## 03. Connect Hardware

Connect one documented controller and the preferred audio interface. Verify the
MIDI input and audio output separately before testing both together. Keep
controller mappings and audio settings in the reference test record.

Success condition: one controller action produces the intended note or control
event, and monitoring stays on the selected interface.

## 04. Export And Reopen

Export a short audio render with explicit sample rate and format settings.
Reopen the project and play the exported file on the same OS. Repeat the test
on the other OS only after its baseline is working.

Success condition: the render is reproducible and the project state remains
usable after reopening.
