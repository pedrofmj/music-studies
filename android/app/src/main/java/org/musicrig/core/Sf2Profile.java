package org.musicrig.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class Sf2Profile {
    public static final String ANDROID_ORGAN_ID = "android-organ-sf2";
    public static final String ANDROID_ORGAN_LABEL =
            "Android Organ (single SF2 layer; not setBfree drawbars)";

    public final String id;
    public final String label;
    public final String mappingDescription;
    public final List<SoundFont> soundFonts;
    public final short[] patchIndexes;
    public final byte[] activeLayers;
    public final byte[] mapChannels;
    public final int volumeOutputCc;
    public final int reverbOutputCc;
    public final List<SharedSf2Plan.Route> routes;

    public Sf2Profile(String id, String label, List<SoundFont> soundFonts,
                      short[] patchIndexes, byte[] activeLayers,
                      byte[] mapChannels, int volumeOutputCc, int reverbOutputCc,
                      String mappingDescription) {
        if (id == null || label == null || mappingDescription == null ||
                soundFonts == null || soundFonts.isEmpty() ||
                soundFonts.size() > SharedSf2Plan.LAYER_COUNT ||
                patchIndexes == null || patchIndexes.length != SharedSf2Plan.LAYER_COUNT ||
                activeLayers == null || mapChannels == null ||
                mapChannels.length != SharedSf2Plan.LAYER_COUNT) {
            throw new IllegalArgumentException("Invalid SF2 profile");
        }
        this.id = id;
        this.label = label;
        this.mappingDescription = mappingDescription;
        this.soundFonts = Collections.unmodifiableList(new ArrayList<>(soundFonts));
        this.patchIndexes = patchIndexes.clone();
        this.activeLayers = activeLayers.clone();
        this.mapChannels = mapChannels.clone();
        this.volumeOutputCc = volumeOutputCc;
        this.reverbOutputCc = reverbOutputCc;
        this.routes = Collections.unmodifiableList(SharedSf2Plan.build(
                this.patchIndexes, soundFonts.size(), this.activeLayers));
        for (int channel : this.mapChannels) {
            if ((channel & 0xff) < 1 || (channel & 0xff) > 16) {
                throw new IllegalArgumentException("Invalid MIDI map channel");
            }
        }
    }

    public static Sf2Profile androidOrgan(String soundFontPath, String presetLabel,
                                          byte[] mapChannels, int volumeOutputCc,
                                          int reverbOutputCc) {
        List<SoundFont> soundFonts = new ArrayList<>();
        soundFonts.add(new SoundFont("organ-gospel", soundFontPath, presetLabel));
        return new Sf2Profile(ANDROID_ORGAN_ID, ANDROID_ORGAN_LABEL, soundFonts,
                new short[SharedSf2Plan.LAYER_COUNT], new byte[]{1}, mapChannels,
                volumeOutputCc, reverbOutputCc,
                "One JF Gospel Organ SF2 preset on layer 1. Fader 1 controls channel volume; " +
                        "knob 1 controls reverb. Faders are not tonewheel drawbars, and setBfree " +
                        "percussion/rotary controls are not emulated.");
    }

    public int outputChannel(SharedSf2Plan.Route route) {
        return mapChannels[route.mixerSlot - 1] & 0xff;
    }

    public static final class SoundFont {
        public final String id;
        public final String path;
        public final String label;

        public SoundFont(String id, String path, String label) {
            if (id == null || path == null || label == null) {
                throw new IllegalArgumentException("SoundFont id, path, and preset label are required");
            }
            this.id = id;
            this.path = path;
            this.label = label;
        }
    }
}
