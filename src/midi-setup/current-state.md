# Current State

## Machine Baseline

| Item | Verified state |
| --- | --- |
| Host | airstar, Ubuntu 24.04.4 LTS, x86_64 |
| Audio/MIDI | PipeWire 1.0.5, WirePlumber 0.4.17, PipeWire Pulse |
| Plugin host | Carla Flatpak 2.5.10, studio.kx.carla |
| Project | /c/music/carla/pedro.uproject |
| Rack structure | 49 uniquely named plugins, 16 native SF2 slots, 111 Carla project connections |
| Saved graph | 118 raw links; 116 performance-owned deployment links; 68 MIDI links |
| Quantum | 2048 frames, persisted by pipewire-carla-quantum.service |
| Output | Tiger Lake Speaker + Headphones through the rack EQ and Arturia gate |

The project stored in Git and the deployed project had SHA-256
48ea4f777c68ebd39105902739258795edfa96fefd8820b62fb31d4efe329974
at capture.

## Controllers

- Arturia KeyLab Essential 61 mk3: nine instrument layers, reverb controls,
  central master-volume encoder, and Carla-only mute click.
- M-VAVE SMK-25: eight independently latchable pad instruments, eight volume
  knobs, and MCP Play/Stop through the AUX port.
- M-VAVE SMC-Mixer: eight faders mapped through CC intermediaries to the
  eight-band LSP equalizer.
- M-VAVE SMC-PAD and SMC-PAD Pocket: both feed the PD-CH-1 drum SoundFont.

All M-VAVE devices expose the same USB product ID and several SINCO endpoints.
The saved graph resolves semantic PipeWire aliases, not changing global port
IDs. The SMK AUX alias ends in whitespace; do not normalize it manually.

## Instrument And Audio Flow

~~~text
Arturia -> AR Controls -> AR-CH-1..9 -----------+
SMC-PAD/Pocket -> PD Controls -> PD-CH-1 --------+-> LSP Mixer x8 Stereo
SMK-25 -> SMK Pad Layers -> SMK-CH-1..8 -> SMK Layer Mixer x8 --+
                                                                 |
LSP Mixer -> SMC-MIX 8-Band EQ -> Arturia volume/mute gate
          -> Speaker + Headphones
~~~

AR-CH-2 has a dedicated +6 dB trim before the master mixer. SMK Knobs 1-8
control hard mixer gains for their corresponding layers. The Arturia central
encoder is converted from relative CC114 to persistent absolute CC119. The same
helper sends channel-10 CC7=127 once whenever its dedicated output connects or
reconnects to the drum SoundFont, preventing retained zero volume from silencing
both pad devices.

## Active User Services

- pipewire-patchbay-watch.service
- smk25-pad-layers.service
- arturia-main-volume-encoder.service
- pipewire-carla-quantum.service

All four were enabled and active at capture.

## External Dependencies

The direct project assets are 16 SoundFonts plus DecentSampler.so. The two
DecentSampler slots additionally require Basic Piano.dsbundle and DS + VT -
altstrings Free Edition.dsbundle. Exact paths, byte sizes, and hashes are in
[setup.json](../../docs/tools/airstar-live-setup/setup.json).

Carla Flatpak must see lsp-plugins.lv2 and midifilter.lv2. The reproducible
installer copies the package-owned bundles to ~/.lv2; it does not depend on
a machine-specific document-portal ID.
