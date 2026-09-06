#!/usr/bin/env python3
"""Render a rack-inspired complementary melody over a reference MP3."""

import math
import struct
import subprocess
import sys
import wave
from pathlib import Path


REFERENCE = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt/novabase.mp3")
OUTPUT = Path("/c/development/egt/customers/ped/music-studies/novabase-with-melody.mp3")
WAV = Path("/tmp/novabase-with-melody.wav")
SAMPLE_RATE = 44100
DURATION = 31.43


def note_frequency(note: int) -> float:
    return 440.0 * 2.0 ** ((note - 69) / 12.0)


def envelope(position: float, length: float) -> float:
    attack = min(0.035, length * 0.2)
    release = min(0.18, length * 0.35)
    if position < attack:
        return position / attack
    if position > length - release:
        return max(0.0, (length - position) / release)
    return 1.0


def render_melody(path: Path) -> None:
    # A restrained A-minor phrase, voiced like the rack's synth/pad layers.
    bpm = 112.0
    beat = 60.0 / bpm
    phrase = [
        (0.0, 69, 0.5), (0.5, 72, 0.5), (1.0, 74, 0.75), (1.75, 72, 0.25),
        (2.0, 69, 0.5), (2.5, 67, 0.5), (3.0, 64, 0.75), (3.75, 67, 0.25),
        (4.0, 69, 0.5), (4.5, 72, 0.5), (5.0, 76, 0.75), (5.75, 74, 0.25),
        (6.0, 72, 0.5), (6.5, 69, 0.5), (7.0, 67, 1.0),
    ]
    samples = [0.0] * int(DURATION * SAMPLE_RATE)
    for repeat in range(4):
        for start_beat, midi, length_beats in phrase:
            start = (repeat * 8.0 + start_beat) * beat
            length = length_beats * beat
            if start >= DURATION:
                continue
            frequency = note_frequency(midi)
            first = int(start * SAMPLE_RATE)
            last = min(len(samples), int((start + length) * SAMPLE_RATE))
            for index in range(first, last):
                t = index / SAMPLE_RATE - start
                amp = 0.16 * envelope(t, length)
                # Layered synth fundamental, octave shimmer, and soft fifth.
                value = (
                    math.sin(2 * math.pi * frequency * t)
                    + 0.28 * math.sin(2 * math.pi * frequency * 2 * t)
                    + 0.12 * math.sin(2 * math.pi * frequency * 1.5 * t)
                ) * amp
                samples[index] += value
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for value in samples:
            value = max(-0.9, min(0.9, value))
            packed = struct.pack("<h", int(value * 32767))
            frames.extend(packed)
            frames.extend(packed)
        output.writeframes(frames)


def main() -> int:
    reference = Path(sys.argv[1]) if len(sys.argv) > 1 else REFERENCE
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else OUTPUT
    render_melody(WAV)
    subprocess.run([
        "ffmpeg", "-y", "-i", str(reference), "-i", str(WAV),
        "-filter_complex", "[0:a]volume=1.0[ref];[1:a]aecho=0.8:0.7:90:0.25,volume=1.8[mel];[ref][mel]amix=inputs=2:duration=first:dropout_transition=2,alimiter=limit=0.95",
        "-c:a", "libmp3lame", "-b:a", "192k", str(output),
    ], check=True)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
