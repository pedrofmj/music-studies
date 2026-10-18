package org.musicrig.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class RigHost implements AutoCloseable {
    private static final int MASTER_VOLUME_CC = 114;
    private static final int MASTER_MUTE_CC = 115;

    private final SynthEngine synth;
    private final SoundFontRepository soundFonts;
    private final PresetResolver presetResolver;
    private final ArturiaControlState controls = new ArturiaControlState();
    private final Map<String, String> materializedPaths = new HashMap<>();
    private final Map<String, LoadedSoundFont> loadedSoundFonts = new HashMap<>();
    private final Map<String, Map<String, Sf2Preset>> resolvedPresets = new HashMap<>();
    private Sf2Profile profile;
    private int masterVolume = 127;
    private boolean masterMuted;
    private boolean encoderClickPressed;

    public RigHost(SynthEngine synth, SoundFontRepository soundFonts) {
        this(synth, soundFonts, new ExactPresetResolver());
    }

    public RigHost(SynthEngine synth, SoundFontRepository soundFonts,
                   PresetResolver presetResolver) {
        if (synth == null || soundFonts == null || presetResolver == null) {
            throw new IllegalArgumentException("Synth, SoundFont repository, and preset resolver are required");
        }
        this.synth = synth;
        this.soundFonts = soundFonts;
        this.presetResolver = presetResolver;
        synth.setMasterVolume(masterVolume);
        synth.setMasterMuted(false);
    }

    public synchronized void selectProfile(Sf2Profile nextProfile) throws Exception {
        int count = nextProfile.soundFonts.size();
        int[] soundFontIds = new int[count];
        int[] presetBanks = new int[count];
        int[] presetPrograms = new int[count];
        for (int index = 0; index < count; index++) {
            Sf2Profile.SoundFont patch = nextProfile.soundFonts.get(index);
            LoadedSoundFont loaded = loadSoundFont(patch.path);
            Sf2Preset preset = resolvePreset(patch, loaded);
            soundFontIds[index] = loaded.id;
            presetBanks[index] = preset.bank;
            presetPrograms[index] = preset.program;
        }

        profile = null;
        synth.configureProfile(nextProfile, soundFontIds, presetBanks, presetPrograms);
        profile = nextProfile;
        replayControls();
    }

    /** Loads and indexes a profile's SoundFonts without changing the active profile. */
    public synchronized void preloadProfile(Sf2Profile nextProfile) throws Exception {
        for (Sf2Profile.SoundFont patch : nextProfile.soundFonts) {
            LoadedSoundFont loaded = loadSoundFont(patch.path);
            resolvePreset(patch, loaded);
        }
    }

    public synchronized boolean onMidiMessage(byte[] message) {
        if (message == null || message.length < 2) {
            return false;
        }
        int status = message[0] & 0xff;
        if (status == 0xB0 && message.length >= 3) {
            int controller = message[1] & 0x7f;
            int value = message[2] & 0x7f;
            if (controller == MASTER_VOLUME_CC) {
                setMasterVolume(masterVolume + value - 64);
                return true;
            }
            if (controller == MASTER_MUTE_CC) {
                if (value != 0 && !encoderClickPressed) {
                    masterMuted = !masterMuted;
                    synth.setMasterMuted(masterMuted);
                }
                encoderClickPressed = value != 0;
                return true;
            }

            int controlIndex = controls.indexOf(controller);
            if (controls.update(status, controller, value)) {
                synth.observeControllerMessage(message);
                if (profile != null) {
                    sendControlToMatchingRoute(controlIndex, controller, value);
                }
                return true;
            }
        }
        if (profile == null || status >= 0xF0) {
            return false;
        }
        for (SharedSf2Plan.Route route : profile.routes) {
            byte[] routed = message.clone();
            routed[0] = (byte) ((status & 0xF0) | (profile.outputChannel(route) - 1));
            synth.sendMidi(routed);
        }
        return true;
    }

    public synchronized int masterVolume() {
        return masterVolume;
    }

    public synchronized boolean masterMuted() {
        return masterMuted;
    }

    private LoadedSoundFont loadSoundFont(String catalogPath) throws Exception {
        LoadedSoundFont loaded = loadedSoundFonts.get(catalogPath);
        if (loaded == null) {
            String materializedPath = materializedPaths.get(catalogPath);
            if (materializedPath == null) {
                materializedPath = soundFonts.materialize(catalogPath);
                materializedPaths.put(catalogPath, materializedPath);
            }
            int id = synth.loadSoundFont(materializedPath);
            if (id < 0) {
                throw new IllegalStateException("Could not load SoundFont: " + catalogPath);
            }
            loaded = new LoadedSoundFont(id);
            loadedSoundFonts.put(catalogPath, loaded);
        }
        if (loaded.presets == null) {
            List<Sf2Preset> presets = synth.listPresets(loaded.id);
            if (presets == null) {
                throw new IllegalStateException("FluidSynth returned no preset list for " +
                        catalogPath);
            }
            loaded.presets = Collections.unmodifiableList(new ArrayList<>(presets));
        }
        return loaded;
    }

    private Sf2Preset resolvePreset(Sf2Profile.SoundFont soundFont,
                                    LoadedSoundFont loaded) {
        Map<String, Sf2Preset> presetsByLabel = resolvedPresets.get(soundFont.path);
        if (presetsByLabel == null) {
            presetsByLabel = new HashMap<>();
            resolvedPresets.put(soundFont.path, presetsByLabel);
        }
        Sf2Preset preset = presetsByLabel.get(soundFont.label);
        if (preset == null) {
            preset = presetResolver.resolve(soundFont.label, loaded.presets);
            if (preset == null) {
                throw new IllegalStateException("Preset resolver returned no preset for " +
                        soundFont.label);
            }
            presetsByLabel.put(soundFont.label, preset);
        }
        return preset;
    }

    private void setMasterVolume(int volume) {
        int clamped = Math.max(0, Math.min(127, volume));
        if (clamped != masterVolume) {
            masterVolume = clamped;
            synth.setMasterVolume(masterVolume);
        }
    }

    private void sendControlToMatchingRoute(int controlIndex, int controller, int value) {
        for (SharedSf2Plan.Route route : profile.routes) {
            int outputCc = -1;
            if (controlIndex < 9 && route.faderCc == controller) {
                outputCc = profile.volumeOutputCc;
            } else if (controlIndex >= 9 && route.knobCc == controller) {
                outputCc = profile.reverbOutputCc;
            }
            if (outputCc >= 0) {
                sendLayerControl(route, outputCc, value);
            }
        }
    }

    private void replayControls() {
        for (SharedSf2Plan.Route route : profile.routes) {
            int fader = controls.indexOf(route.faderCc);
            int knob = controls.indexOf(route.knobCc);
            sendLayerControl(route, profile.volumeOutputCc, controls.valueAt(fader));
            sendLayerControl(route, profile.reverbOutputCc, controls.valueAt(knob));
        }
    }

    private void sendLayerControl(SharedSf2Plan.Route route, int outputCc, int value) {
        int channel = profile.outputChannel(route);
        synth.sendMidi(new byte[]{(byte) (0xB0 | (channel - 1)),
                (byte) outputCc, (byte) value});
    }

    @Override
    public synchronized void close() {
        synth.close();
        loadedSoundFonts.clear();
        materializedPaths.clear();
        resolvedPresets.clear();
        profile = null;
    }

    private static final class LoadedSoundFont {
        final int id;
        List<Sf2Preset> presets;

        LoadedSoundFont(int id) {
            this.id = id;
        }
    }
}
