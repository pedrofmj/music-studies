# MIDI Routing Boundaries

Sources: [PipeWire MIDI support](https://docs.pipewire.org/page_midi.html) and
[PipeWire session-manager documentation](https://docs.pipewire.org/page_session_manager.html).

PipeWire exposes MIDI devices and streams as processing nodes and ports. The
session manager creates the ALSA sequencer bridge that makes those ports
available and controls policy for device opening and links.

## What This Establishes

- A USB controller can appear as a MIDI source and a software instrument can
  appear as a MIDI sink in the PipeWire graph.
- JACK MIDI clients are supported through PipeWire's compatibility path.
- Devices may appear and disappear as they are connected or disconnected.
- The visible graph is not a confirmed musical route: the documented session
  manager does not automatically link control messages.

## Verification Rule

For any controller, record the ALSA client and port, PipeWire or JACK-visible
port, target instrument, selected channel, and successful note test. For the
KeyLab, test the standard MIDI endpoint before enabling DAW-control ports. For
a direct KeyLab-to-XPS-30 path, PipeWire is not involved.
