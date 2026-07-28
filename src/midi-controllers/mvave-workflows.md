# M-VAVE Shared Workflows

Use this document for the shared behavior of the SMK25, SMC-Mixer, SMC-PAD,
SMC-PAD Pocket, and FM-1. Device-specific controls and workflows are in the corresponding controller study folders.

## Connection Order

1. Start with one controller and one USB data cable.
2. Confirm that the operating system creates a MIDI endpoint before opening a
   DAW or editor.
3. Confirm note, CC, transport, or Mackie Control behavior in a small test
   project.
4. Record the exact endpoint name and target in that controller mapping log.
5. Add BLE MIDI only after the wired path is repeatable.
6. Add the next controller and check that its MIDI channel, notes, and DAW
   surface assignments do not collide.

## Linux Baseline

The official M-VAVE manuals document Windows, macOS, iOS, and Android. They do
not certify Ubuntu, so treat Linux as a documented verification path. USB is
the first choice because all five manuals describe automatic USB recognition on
host computers.

Useful checks:

```bash
lsusb
aconnect -l
amidi -l
```

`aconnect` and `amidi` are supplied by `alsa-utils`. Record the actual client,
port, and connection result in the device mapping log. If the device does not
appear after reconnecting it, test another known-good USB data cable before
changing device settings.

## BLE MIDI

Use BLE only after wired USB works. Pair one device at a time, then verify that
the host creates a usable MIDI endpoint. Bluetooth pairing alone does not prove
that the DAW has a MIDI input. For diagnosis:

```bash
bluetoothctl devices
bluetoothctl info <device-address>
aconnect -l
```

The vendor provides Windows/macOS editor downloads and, for some products,
mobile apps. Do not assume that those editors run on Ubuntu. Retain custom
mappings in this repository so the hardware can be rebuilt without the editor.

## M-VAVE Roles With The XPS-30

- SMK25: compact keyboard input, pads, encoders, transport, and optional MIDI
  control of a computer or hardware synth.
- SMC-PAD Pocket: lightweight sample, clip, or scene triggering.
- SMC-PAD: richer pad performance, encoder control, transport, and wired MIDI
  out through its 3.5 mm output.
- SMC-Mixer: DAW control surface. It controls a DAW mixer; it is not an audio
  mixer and does not mix XPS-30 audio on its own.
- FM-1: compact FM sound source and MIDI controller. Its headphone output needs
  an audio path to a mixer or audio interface; MIDI connection alone carries no audio.

## Collision Rules

- Give each controller a documented MIDI channel where the target requires one.
- Do not use the same notes for pads that trigger unrelated samples or clips.
- Keep Mackie Control endpoints dedicated to one DAW surface configuration.
- Save a known-good mapping before changing a vendor editor or firmware.
- Label any wireless adapter used for MIDI out; vendor manuals say those
  adapters are optional accessories and may reserve the wireless link.
