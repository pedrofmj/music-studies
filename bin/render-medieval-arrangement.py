#!/usr/bin/env python3
"""Render a five-minute modal, medieval-inspired version of the reference."""

import math
import struct
import subprocess
import wave
from pathlib import Path

SR = 44100
DURATION = 300.0
REFERENCE = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt/novabase.mp3")
FOLDER = Path("/home/ldap/pedro.ferreira/Músicas/Gravações/qt/novabase-medieval")
SYNTH = Path("/tmp/novabase-medieval.wav")


def freq(note):
    return 440.0 * 2 ** ((note - 69) / 12)


def env(t, length, attack=0.03, release=0.15):
    if t < attack:
        return t / attack
    if t > length - release:
        return max(0.0, (length - t) / release)
    return 1.0


def note(buf, start, length, midi, level, voice):
    f = freq(midi)
    for i in range(max(0, int(start * SR)), min(len(buf), int((start + length) * SR))):
        t = i / SR - start
        e = env(t, length, 0.18 if voice == "drone" else 0.025, 0.3 if voice == "drone" else 0.12)
        if voice == "lute":
            value = math.sin(2 * math.pi * f * t) + 0.32 * math.sin(2 * math.pi * f * 2 * t) + 0.12 * math.sin(2 * math.pi * f * 3 * t)
        elif voice == "flute":
            value = math.sin(2 * math.pi * f * t) + 0.08 * math.sin(2 * math.pi * f * 2 * t)
        elif voice == "drone":
            value = math.sin(2 * math.pi * f * t) + 0.2 * math.sin(2 * math.pi * f * 0.5 * t)
        else:
            value = math.sin(2 * math.pi * f * t)
        buf[i] += level * e * value


def write_wav(path, buf):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(SR)
        frames = bytearray()
        for value in buf:
            frame = struct.pack("<h", int(max(-0.85, min(0.85, value)) * 32767))
            frames.extend(frame)
            frames.extend(frame)
        out.writeframes(frames)


def render():
    FOLDER.mkdir(parents=True, exist_ok=True)
    layers = {name: [0.0] * int(DURATION * SR) for name in ("drone", "lute", "flute", "bells", "percussion", "gregorian-choir")}
    beat = 60.0 / 96.0
    bar = 8 * beat
    modes = [(57, 60, 62, 64), (53, 57, 60, 62), (48, 52, 55, 57), (55, 59, 62, 64)]
    melody = [69, 72, 74, 72, 69, 67, 64, 67, 69, 72, 76, 74, 72, 69, 67, 64]
    for section in range(37):
        start = section * bar
        mode = modes[section % len(modes)]
        root = mode[0]
        if section > 0:
            note(layers["drone"], start, bar * 0.98, root - 12, 0.07, "drone")
        if section >= 2:
            for step in range(16):
                n = mode[(step + section) % 4] + (12 if step % 8 >= 4 else 0)
                note(layers["lute"], start + step * beat / 2, beat * 0.38, n, 0.045, "lute")
        if section >= 4:
            for step, n in enumerate(melody):
                if section % 6 == 5 and step % 4 == 3:
                    continue
                note(layers["flute"], start + step * beat / 2, beat * 0.43, n + (12 if section in (12, 13, 24, 25) else 0), 0.1, "flute")
        if section >= 8:
            for step in (1, 5, 9, 13):
                note(layers["bells"], start + step * beat / 2, beat * 0.18, mode[(step // 4) % 4] + 12, 0.045, "bells")
        if section >= 6:
            for step in range(8):
                note(layers["percussion"], start + step * beat, beat * 0.08, 36 if step in (0, 4) else 43, 0.035, "bells")
        if section >= 3:
            # Sustained modal unison and octave voices emulate a small male chant choir.
            chant_root = root + (12 if section % 8 in (4, 5) else 0)
            note(layers["gregorian-choir"], start, bar * 0.94, chant_root, 0.055, "drone")
            note(layers["gregorian-choir"], start + bar * 0.48, bar * 0.44, mode[2] + (12 if section % 8 in (4, 5) else 0), 0.04, "drone")
    for name, buf in layers.items():
        wav = Path("/tmp") / f"novabase-medieval-{name}.wav"
        mp3 = FOLDER / f"novabase-medieval-{name}.mp3"
        write_wav(wav, buf)
        subprocess.run(["ffmpeg", "-y", "-i", str(wav), "-c:a", "libmp3lame", "-b:a", "192k", str(mp3)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    mixed = [sum(values) for values in zip(*layers.values())]
    write_wav(SYNTH, mixed)
    subprocess.run(["ffmpeg", "-y", "-i", str(REFERENCE), "-i", str(SYNTH), "-filter_complex", "[0:a]aloop=loop=-1:size=2e+09,atrim=duration=300,volume=0.48[base];[1:a]aecho=0.8:0.75:120:0.28,volume=1.7[med];[base][med]amix=inputs=2:duration=first,afade=t=in:st=0:d=4,afade=t=out:st=295:d=5,alimiter=limit=0.95", "-c:a", "libmp3lame", "-b:a", "192k", str(FOLDER / "novabase-medieval-full-with-gregorian-choir.mp3")], check=True)


if __name__ == "__main__":
    render()
