#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v javac >/dev/null 2>&1; then
    printf '%s\n' "A JDK with javac is required to run the mock tests." >&2
    exit 127
fi
CLASSES=$(mktemp -d)
trap 'rm -rf "$CLASSES"' EXIT HUP INT TERM

javac -d "$CLASSES" \
    "$ROOT/app/src/main/java/org/musicrig/core/ArturiaControlState.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/ArturiaPadModes.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/ExactPresetResolver.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/MidiStreamParser.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/PresetResolver.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/SharedSf2Plan.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/Sf2Profile.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/Sf2Preset.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/SoundFontRepository.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/VerifiedSf2Pack.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/SynthEngine.java" \
    "$ROOT/app/src/main/java/org/musicrig/core/RigHost.java" \
    "$ROOT/app/src/test/java/org/musicrig/core/RigHostMockTest.java" \
    "$ROOT/app/src/test/java/org/musicrig/core/VerifiedSf2PackTest.java" \
    "$ROOT/app/src/test/java/org/musicrig/core/VerifiedSf2PackLiveTest.java"
java -ea -cp "$CLASSES" org.musicrig.core.RigHostMockTest
java -ea -cp "$CLASSES" org.musicrig.core.VerifiedSf2PackTest
if [ -n "${MUSIC_RIG_LIVE_SF2_URL:-}" ]; then
    java -ea -cp "$CLASSES" org.musicrig.core.VerifiedSf2PackLiveTest \
        "$MUSIC_RIG_LIVE_SF2_URL"
fi
