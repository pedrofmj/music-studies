# Current State

## Machine Baseline

| Item | Verified state |
| --- | --- |
| Host | airstar, Ubuntu 24.04.4 LTS, x86_64 |
| Audio/MIDI services | PipeWire 1.0.5, WirePlumber 0.4.17, and PipeWire Pulse are active in the pedro.ferreira user session. |
| Plugin host | Carla 2.5.10, installed system-wide from Flathub as studio.kx.carla. |
| Active project | /c/music/carla/pedro.uproject, 2026-08-02 snapshot. |
| Default audio output | Tiger Lake-LP Smart Sound Technology Audio Controller Speaker + Headphones. |
| External audio interface | Not present in the live PipeWire graph during verification. |

The Carla Flatpak sees home-directory plugins directly. System LV2 plugins are
available through the existing XDG document-portal grant at
/run/user/50001/doc/516068e9/lv2; Carla's configured LV2 path includes that
grant. Do not replace it with a direct /usr/lib/lv2 Flatpak override: Flatpak
reserves /usr, while the portal grant is working and exposes installed bundles.

## Detected MIDI Inputs

- Midi-Bridge:KL Essential 61 mk3 3:(capture_0) KL Essential 61 mk3 MIDI
- Midi-Bridge:USB Composite Device 5:(capture_0) USB Composite Device MIDI 1
- SMC-PAD, SMK25, SMC-Mixer, and SMC-PAD Pocket ports exposed as SINCO MIDI
  endpoints.

The KeyLab has additional DIN-thru, MCU/HUI, and ALV ports. The performance
rack uses only the KL Essential 61 mk3 MIDI endpoint, not the DAW-control
ports.

## Verified Carla Graph

~~~text
KeyLab Essential 61 mk3 MIDI
  -> MIDI Scale CC Value (CC 64 transformation)
  -> DecentSampler. Basic Piano
  -> DecentSampler - Alt Strings
  -> ACE Fluid Synth
  -> ACE Fluid Synth (2)
  -> LSP Mixer x8 Stereo
  -> Tiger Lake Speaker + Headphones
~~~

Each instrument's stereo outputs feed the LSP mixer, whose outputs feed the
default Tiger Lake sink. MIDI Scale CC Value is configured for parameter 64
with a negative value scale and is the existing pedal-value transformation.

Fluida is installed on disk but is not active in the current graph. The saved
project has a stale Fluida position label; its active plugin list uses the two
ACE FluidSynth instances instead.

## Verification Limits

- The live PipeWire graph and file visibility were verified over SSH.
- qpwgraph and Audacity were installed successfully but their GUI was not
  opened from SSH, because no graphical display is available in that session.
- Dragonfly and Surge bundles are visible to Carla. They need a normal Carla
  plugin refresh before they appear in its Add Plugin dialog.
