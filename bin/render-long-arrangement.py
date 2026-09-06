#!/usr/bin/env python3
"""Create a five-minute arrangement from a reference loop and synthesized parts."""

import math
import struct
import subprocess
import sys
import wave
from pathlib import Path

SR = 44100
TARGET = 300.0
REFERENCE = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt/novabase.mp3")
OUTPUT = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt/novabase-rich-5min.mp3")
SYNTH_WAV = Path("/tmp/novabase-rich-5min-synth.wav")


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def adsr(t, length, attack=0.025, release=0.12):
    if t < attack:
        return t / attack
    if t > length - release:
        return max(0.0, (length - t) / release)
    return 1.0


def add_note(buf, start, length, midi, level, voice):
    frequency = hz(midi)
    first = max(0, int(start * SR))
    last = min(len(buf), int((start + length) * SR))
    for i in range(first, last):
        t = i / SR - start
        e = adsr(t, length, 0.02 if voice != "pad" else 0.3, 0.2 if voice == "pad" else 0.1)
        if voice == "arp":
            value = math.sin(2 * math.pi * frequency * t) + 0.22 * math.sin(2 * math.pi * frequency * 2 * t)
        elif voice == "pad":
            value = math.sin(2 * math.pi * frequency * t) + 0.35 * math.sin(2 * math.pi * frequency * 0.5 * t)
        elif voice == "counter":
            value = math.sin(2 * math.pi * frequency * t) + 0.18 * math.sin(2 * math.pi * frequency * 3 * t)
        else:
            value = math.sin(2 * math.pi * frequency * t) + 0.28 * math.sin(2 * math.pi * frequency * 2 * t) + 0.1 * math.sin(2 * math.pi * frequency * 3 * t)
        buf[i] += level * e * value


def create_synth(path):
    layers = {
        "pads": [0.0] * int(TARGET * SR),
        "arpeggios": [0.0] * int(TARGET * SR),
        "melody": [0.0] * int(TARGET * SR),
        "countermelody": [0.0] * int(TARGET * SR),
    }
    bpm = 112.0
    beat = 60.0 / bpm
    section = 8 * beat
    chords = [(57, 60, 64), (53, 57, 60), (48, 52, 55), (55, 59, 62)]
    melody = [(0, 69), (0.5, 72), (1, 74), (1.75, 72), (2, 69), (2.5, 67), (3, 64), (3.75, 67), (4, 69), (4.5, 72), (5, 76), (5.75, 74), (6, 72), (6.5, 69), (7, 67)]
    arp_pattern = [0, 1, 2, 1, 0, 1, 2, 1, 2, 1, 0, 1, 2, 1, 0, 1]
    for section_index, section_start in enumerate(x * section for x in range(38)):
        chord = chords[section_index % len(chords)]
        # Evolving pad bed: enter gradually, thin out before each transition.
        if section_index > 1:
            for note in chord:
                add_note(layers["pads"], section_start, section * 0.98, note, 0.035, "pad")
        # Sixteenth-note arpeggio, alternating register and density by section.
        if section_index % 6 != 0:
            for step in range(32):
                note = chord[arp_pattern[step % len(arp_pattern)]] + (12 if (step // 8 + section_index) % 2 else 0)
                add_note(layers["arpeggios"], section_start + step * beat / 4, beat / 3, note, 0.045 + 0.01 * (section_index % 3), "arp")
        # Main phrase, with octave lift and rhythmic variation every eight sections.
        if section_index >= 2:
            for offset, note in melody:
                if section_index % 8 == 7 and offset in (1.75, 3.75, 5.75):
                    continue
                add_note(layers["melody"], section_start + offset * beat, beat * (0.42 if offset % 1 else 0.55), note + (12 if section_index in (11, 12, 19, 20, 27, 28, 35, 36) else 0), 0.105, "lead")
        # A quieter answer phrase adds a second voice in the later sections.
        if section_index >= 8:
            for offset, note in ((1.5, 64), (2.5, 67), (4.5, 69), (6.5, 67)):
                add_note(layers["countermelody"], section_start + offset * beat, beat * 0.42, note + (12 if section_index % 4 == 3 else 0), 0.06, "counter")
    for layer_name, buf in layers.items():
        layer_path = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt") / f"novabase-rich-5min-{layer_name}.wav"
        with wave.open(str(layer_path), "wb") as out:
            out.setnchannels(2)
            out.setsampwidth(2)
            out.setframerate(SR)
            frames = bytearray()
            for value in buf:
                value = max(-0.85, min(0.85, value))
                frame = struct.pack("<h", int(value * 32767))
                frames.extend(frame)
                frames.extend(frame)
            out.writeframes(frames)
        subprocess.run(["ffmpeg", "-y", "-i", str(layer_path), "-c:a", "libmp3lame", "-b:a", "192k", str(layer_path.with_suffix(".mp3"))], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(SR)
        frames = bytearray()
        for value in zip(*layers.values()):
            value = sum(value)
            value = max(-0.85, min(0.85, value))
            frame = struct.pack("<h", int(value * 32767))
            frames.extend(frame)
            frames.extend(frame)
        out.writeframes(frames)


def main():
    reference = Path(sys.argv[1]) if len(sys.argv) > 1 else REFERENCE
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else OUTPUT
    create_synth(SYNTH_WAV)
    filter_graph = (
        "[0:a]aloop=loop=-1:size=2e+09,atrim=duration=300,volume=0.82[base];"
        "[1:a]volume=1.55,aecho=0.8:0.7:85:0.22,alimiter=limit=0.8[parts];"
        "[base][parts]amix=inputs=2:duration=first:dropout_transition=3,"
        "afade=t=in:st=0:d=2,afade=t=out:st=295:d=5,alimiter=limit=0.95"
    )
    subprocess.run(["ffmpeg", "-y", "-i", str(reference), "-i", str(SYNTH_WAV), "-filter_complex", filter_graph, "-c:a", "libmp3lame", "-b:a", "192k", str(output)], check=True)
    print(output)


if __name__ == "__main__":
    main()
