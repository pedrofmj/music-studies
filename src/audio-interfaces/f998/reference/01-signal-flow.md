# Signal Flow And Limits

F998 is a portable creator/streaming sound card, not a conventional multichannel
line mixer. Its documented workflow is microphone plus accompaniment, then a
processed monitoring and streaming feed.

```text
microphone / wired accompaniment / Bluetooth accompaniment
  -> F998 input and processing controls
  -> monitor/headset and claimed phone/USB streaming paths
```

## Appropriate Role

Use F998 for spoken voice, podcasting, live streams, simple karaoke, backing
track playback, and its built-in voice/effect controls. Use the TEYUN A8 for
mixing XPS-30 and FM-1 into a PA or powered speakers.

## Input Safety

The physical F998 input design is unverified. A keyboard output or FM-1
headphone output can be far stronger than an accompaniment input expects. Do
not use the F998 as a direct keyboard line mixer without an observed low-level
test. Start source volume at minimum, increase slowly, listen for distortion,
and retain the tested cable/attenuator result in the routing log.

## Monitoring And Streaming

Headphone monitoring should be established before connecting a phone or
computer. Monitor the microphone and accompaniment balance before adding echo,
voice changing, or sound effects. This prevents a processed stream from hiding
clipping or an inaudible microphone.

## Bluetooth And USB

Bluetooth is audio accompaniment, not MIDI. The claimed OTG/PC port must be
tested for Linux capture and playback separately. Use a conservative one-source,
one-destination test before attempting a combined live stream.
