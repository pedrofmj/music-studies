# Plugin Inventory

This inventory separates package ownership from whether a plugin is active in
the rack. Package ownership was checked with dpkg-query -S against each LV2
bundle's manifest.ttl; apt-get -s autoremove reported no orphan packages.

## Installed And Available

| Component | Version/source | Format or role | Rack state |
| --- | --- | --- | --- |
| Carla | Flatpak 2.5.10 | Plugin host and patchbay | Open with pedro.uproject |
| PipeWire / WirePlumber | Ubuntu 1.0.5 / 0.4.17 | Audio and MIDI graph | Active |
| DecentSampler | Manual ~/.vst/DecentSampler.so | VST2, .dspreset / .dsbundle player | Two active instances |
| Carla native SF2 engine | Included in Carla Flatpak | SF2 player | Sixteen active instrument slots |
| ACE FluidSynth | ardour-lv2-plugins 8.4.0 | LV2, SF2/SF3 player | Installed, not active; Carla native SF2 slots replaced it |
| Fluida | Manual ~/.lv2/Fluida.lv2 | LV2, SF2/SF3 player | Installed, not in active graph |
| LSP Plugins | lsp-plugins-lv2 1.2.14 | LV2 effects, metering, mixer | Master mixer, SMK mixer, output trim, and 8-band EQ active |
| x42 Plugins | x42-plugins 20230915 | LV2 utility and MIDI filters | scalecc and mapcc instances active |
| Dragonfly Reverb | dragonfly-reverb-lv2 3.2.10 | Four LV2 reverbs | Installed; refresh Carla before use |
| Surge XT | Official surge-xt 1.3.4 Debian package | LV2 instrument/effects, VST3, CLAP, standalone | Installed; refresh Carla before use |
| qpwgraph | Flatpak 1.0.3 | PipeWire graph inspection and optional patchbay recovery | Installed |
| REAPER | Flatpak 7.78 | DAW / recording | Installed |
| Ardour | Ubuntu 8.4.0 | DAW / recording | Installed |
| Polyphone | Flatpak 2.6 | SoundFont editor | Installed |
| Audacity | Flatpak 3.7.8 | Quick waveform editing | Installed |

## LV2 Ownership Result

The paths in the request were lvm2, but the relevant plugin format and actual
paths are lv2:

- /usr/lib/lvm2 and ~/.lvm2 do not exist on airstar.
- /usr/lib/lv2 contains package-owned bundles. Ardour owns ACE FluidSynth,
  x42 owns its utility bundles, and LSP owns lsp-plugins.lv2.
- New bundles are package tracked: Dragonfly is owned by
  dragonfly-reverb-lv2 and Surge XT is owned by surge-xt.
- ~/.lv2/Fluida.lv2 is not owned by dpkg, so it is a manual installation.
- ~/.vst/DecentSampler.so is also not owned by dpkg; this is the manually
  installed VST2 that the current project actively uses.

There are no unowned bundles under /usr/lib/lv2 and no packages eligible for
automatic removal. The only manually copied plugin payloads found were Fluida
and DecentSampler in the user home directory.

## Remaining Deliberate Gaps

| Item | Status | Reason and next action |
| --- | --- | --- |
| sfizz | Not installed | Ubuntu 24.04 has no candidate package. The published Ubuntu route is an OBS repository named home:sfztools:sfizz:develop; it was intentionally not added to a production workstation. Prefer a reviewed stable package or a user-local build in ~/.lv2. |
| Vital | Not installed | No Ubuntu package candidate was present. The official Linux download is account-gated, so it cannot be downloaded or licensed on the user's behalf. Install its official Debian package after signing in, then verify the LV2 bundle through Carla. |
| Standalone FluidSynth | Not installed | Not required for this rack: Carla native SF2 slots supply the active SoundFont-player role. |
| MidiMinder | Not installed | This was a product idea in the original suggestion, not an identified package or required component of the current rack. |

Surge XT and DecentSampler satisfy the synthesizer and modern sample-library
roles. Dragonfly, LSP, and x42 cover the recommended effects and utilities.
