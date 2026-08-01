# Audio Software Inventory

This is the Linux and Windows counterpart to the hardware catalog. It records
primary function, native platform availability, source status, license, price
model, and official documentation.

The initial list is curated rather than exhaustive. A product belongs here only
when its current platform and licensing claims have an official-source link.
The machine-readable source of truth is [catalog.json](catalog.json).

## Current Inventory

Status and access claims were reviewed on 2026-07-28. Check the linked official
pages again before installing or buying because platform support and pricing
change.

| Software | Function | Linux | Windows | Source / license | Access |
| --- | --- | --- | --- | --- | --- |
| [Ardour](https://community.ardour.org/download) | DAW: record, edit, route, mix, automate audio/MIDI | Native | Native | Open source, GPL-2.0 | Free source/distro builds; paid ready-to-run builds |
| [REAPER](https://www.reaper.fm/download.php) | DAW: recording, editing, mixing, MIDI, automation, scripting | Native | Native | Proprietary | Paid; 60-day full trial |
| [Bitwig Studio](https://www.bitwig.com/download/) | DAW: clip/linear production, instruments, effects, modulation | Native | Native | Proprietary | Paid editions; 30-day trial |
| [LMMS](https://lmms.io/download) | Pattern DAW: MIDI composition, instruments, effects, export | Native | Native | Open source, GPL-2.0-or-later | Free |
| [Qtractor](https://www.qtractor.org/) | DAW: multitrack audio/MIDI recording and clip composition | Native | Not offered | Open source, GPL-2.0-or-later | Free |
| [Audacity](https://www.audacityteam.org/download/) | Audio editor/recorder: capture, edit, restore, process, export | Native | Native | Open source, GPL-2.0 | Free |
| [Carla](https://kx.studio/Applications:Carla) | Plugin rack/host and audio/MIDI patchbay | Native | Native | Open source, GPL-2.0 | Free |
| [VCV Rack](https://vcvrack.com/downloads/) | Virtual modular synthesizer and patchable signal environment | Native | Native | Open source, GPL-3.0 | Free base; paid Rack Pro/third-party modules |
| [PipeWire](https://pipewire.org/) | Low-latency Linux audio/MIDI graph and routing service | Native | Not offered | Free software | Free |
| [EasyEffects](https://github.com/wwmm/easyeffects) | PipeWire effects chains: EQ, compression, limiting, noise tools | Native | Not offered | Open source, GPL-3.0-or-later | Free |
| [Equalizer APO](https://sourceforge.net/projects/equalizerapo/) | Windows system-wide parametric equalizer/filter engine | Not offered | Native | Open source, GPL-2.0 | Free |
| [yabridge](https://github.com/robbert-vdh/yabridge) | Linux bridge for Windows VST2/VST3/CLAP plugins | Native | N/A | Open source, GPL-3.0-or-later | Free |
| [OBS Studio](https://obsproject.com/download) | Audio/video scene mixing, recording, and live streaming | Native | Native | Open source, GPL-2.0-or-later | Free |

Native means that a current official native build or support statement exists
for that OS. It does not promise that a specific interface driver, plugin,
distribution, or hardware setup works. N/A means yabridge runs on Linux and
bridges Windows plugin binaries through Wine; it is not a Windows application.

## Manual And Reference Structure

When an application needs active study notes, copy
[software-template](software-template/README.md) to
src/audio-software/<software-id>/.

~~~text
src/audio-software/<software-id>/
  README.md
  profile.json
  reference/
    README.md
  manual/
    README.md
    manuals.json
    source/
      README.md
      official-manual.pdf
    extracted/
      README.md
      official-manual.txt
~~~

The original PDF remains authoritative. Its matching text file is a local
search layer made with pdftotext -layout. Retain manuals only when the
internal-study scope allows it; otherwise store the official documentation URL
without saving a copy.

Read [schema.md](schema.md) before adding inventory records and
[manual-ingestion.md](manual-ingestion.md) before retaining or extracting a
manual. Keep real compatibility tests in the individual application reference
directory, with OS, app version, audio backend, device, driver, plugin format,
and result.

## Search

    rg -n daw src/audio-software
    rg -n -i ASIO src/audio-software/*/manual/extracted
