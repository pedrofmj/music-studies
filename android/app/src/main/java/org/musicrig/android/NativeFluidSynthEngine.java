package org.musicrig.android;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Process;

import org.musicrig.core.Sf2Profile;
import org.musicrig.core.Sf2Preset;
import org.musicrig.core.SynthEngine;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class NativeFluidSynthEngine implements SynthEngine {
    private static final int SAMPLE_RATE = 48000;
    private static final int FRAMES_PER_BLOCK = 256;
    private static final boolean LIBRARY_AVAILABLE;

    static {
        boolean available;
        try {
            System.loadLibrary("music_rig_fluidsynth");
            available = true;
        } catch (UnsatisfiedLinkError error) {
            available = false;
        }
        LIBRARY_AVAILABLE = available;
    }

    private long nativeHandle;
    private final AudioTrack audioTrack;
    private final short[] renderBuffer = new short[FRAMES_PER_BLOCK * 2];
    private volatile boolean running;
    private boolean closed;
    private int masterVolume = 127;
    private boolean masterMuted;
    private Thread renderThread;

    public static boolean isAvailable() {
        return LIBRARY_AVAILABLE;
    }

    public NativeFluidSynthEngine() {
        if (!LIBRARY_AVAILABLE) {
            throw new IllegalStateException("FluidSynth native bundle is not configured; " +
                    "build with -PfluidsynthRoot=<Android FluidSynth bundle>");
        }
        nativeHandle = nativeCreate(SAMPLE_RATE);
        if (nativeHandle == 0) {
            throw new IllegalStateException("FluidSynth initialization failed");
        }
        nativeSetMasterState(nativeHandle, masterVolume, false);
        int minimumBytes = AudioTrack.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
        if (minimumBytes <= 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
            throw new IllegalStateException("Android AudioTrack does not support stereo PCM output");
        }
        audioTrack = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build())
                .setAudioFormat(new AudioFormat.Builder()
                        .setSampleRate(SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .build())
                .setBufferSizeInBytes(Math.max(minimumBytes, renderBuffer.length * 2 * 4))
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build();
        if (audioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
            audioTrack.release();
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
            throw new IllegalStateException("Android AudioTrack initialization failed");
        }
        startAudio();
    }

    @Override
    public synchronized int loadSoundFont(String materializedPath) {
        int id = nativeLoadSoundFont(nativeHandle, materializedPath);
        if (id < 0) {
            throw new IllegalStateException("FluidSynth could not load SoundFont: " +
                    materializedPath);
        }
        return id;
    }

    @Override
    public synchronized List<Sf2Preset> listPresets(int soundFontId) {
        String[] entries = nativeGetPresets(nativeHandle, soundFontId);
        if (entries == null || entries.length % 3 != 0) {
            throw new IllegalStateException("FluidSynth preset enumeration failed for SoundFont " +
                    soundFontId);
        }
        List<Sf2Preset> presets = new ArrayList<>();
        for (int index = 0; index < entries.length; index += 3) {
            presets.add(new Sf2Preset(entries[index], Integer.parseInt(entries[index + 1]),
                    Integer.parseInt(entries[index + 2])));
        }
        return Collections.unmodifiableList(presets);
    }

    @Override
    public synchronized void configureProfile(Sf2Profile profile, int[] soundFontIds,
                                              int[] presetBanks, int[] presetPrograms) {
        if (soundFontIds.length != profile.soundFonts.size() ||
                presetBanks.length != soundFontIds.length ||
                presetPrograms.length != soundFontIds.length ||
                !nativeApplyProfile(nativeHandle, soundFontIds, profile.patchIndexes,
                        (short) soundFontIds.length, profile.activeLayers,
                        profile.mapChannels, presetBanks, presetPrograms)) {
            throw new IllegalStateException("Portable shared-SF2 route or preset selection was rejected");
        }
    }

    @Override
    public synchronized void sendMidi(byte[] message) {
        if (nativeHandle != 0) {
            nativeSendMidi(nativeHandle, message);
        }
    }

    @Override
    public synchronized void observeControllerMessage(byte[] message) {
        if (nativeHandle != 0) {
            nativeObserveControllerMessage(nativeHandle, message);
        }
    }

    @Override
    public synchronized void setMasterVolume(int volume) {
        masterVolume = Math.max(0, Math.min(127, volume));
        if (nativeHandle != 0) {
            nativeSetMasterState(nativeHandle, masterVolume, masterMuted);
        }
    }

    @Override
    public synchronized void setMasterMuted(boolean muted) {
        masterMuted = muted;
        if (nativeHandle != 0) {
            nativeSetMasterState(nativeHandle, masterVolume, masterMuted);
        }
    }

    private void startAudio() {
        audioTrack.play();
        running = true;
        renderThread = new Thread(() -> {
            Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
            while (running) {
                synchronized (NativeFluidSynthEngine.this) {
                    if (nativeHandle == 0 || !nativeRender(nativeHandle, renderBuffer)) {
                        break;
                    }
                }
                int written = audioTrack.write(renderBuffer, 0, renderBuffer.length,
                        AudioTrack.WRITE_BLOCKING);
                if (written < 0) {
                    break;
                }
            }
        }, "FluidSynth-AudioTrack");
        renderThread.start();
    }

    @Override
    public void close() {
        synchronized (this) {
            if (closed) {
                return;
            }
            closed = true;
        }
        running = false;
        try {
            audioTrack.stop();
        } catch (IllegalStateException ignored) {
            // AudioTrack may already have stopped after an output error.
        }
        if (renderThread != null) {
            renderThread.interrupt();
            try {
                renderThread.join(1000);
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
            }
        }
        audioTrack.pause();
        audioTrack.flush();
        audioTrack.release();
        synchronized (this) {
            if (nativeHandle != 0) {
                nativeDestroy(nativeHandle);
                nativeHandle = 0;
            }
        }
    }

    private static native long nativeCreate(int sampleRate);
    private static native void nativeDestroy(long handle);
    private static native int nativeLoadSoundFont(long handle, String path);
    private static native boolean nativeApplyProfile(long handle, int[] soundFontIds,
            short[] patchIndexes, short patchCount, byte[] activeLayers, byte[] mapChannels,
            int[] presetBanks, int[] presetPrograms);
    private static native String[] nativeGetPresets(long handle, int soundFontId);
    private static native void nativeSendMidi(long handle, byte[] message);
    private static native void nativeObserveControllerMessage(long handle, byte[] message);
    private static native void nativeSetMasterState(long handle, int volume, boolean muted);
    private static native boolean nativeRender(long handle, short[] stereoSamples);
}
