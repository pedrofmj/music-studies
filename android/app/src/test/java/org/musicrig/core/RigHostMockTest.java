package org.musicrig.core;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class RigHostMockTest {
    public static void main(String[] arguments) throws Exception {
        verifiesOrganDefaultsAndPortableControlList();
        verifiesPadModeSelection();
        verifiesSharedRoutePlan();
        verifiesProfileReplayMasterControlsAndLoadOnce();
        verifiesPresetResolutionFailsClosed();
        verifiesPresetResolverInjection();
        verifiesMidiStreamRunningStatus();
    }

    private static void verifiesPadModeSelection() {
        check(ArturiaPadModes.managementPadCode(new byte[]{(byte) 0x99, 40, 127}) == 1,
                "Bank A Pad 1 was not identified");
        check(Sf2Profile.ANDROID_ORGAN_ID.equals(ArturiaPadModes.profileForCode(1)),
                "Bank A Pad 1 did not select Android Organ");
        check("worship-piano".equals(ArturiaPadModes.profileForCode(4)),
                "Bank A Pad 4 did not select Worship Piano");
        check("acoustic-worship".equals(ArturiaPadModes.profileForCode(16)),
                "Bank B Pad 8 did not select Acoustic Worship");
        check(!ArturiaPadModes.isPress(new byte[]{(byte) 0x89, 40, 0}),
                "Pad release was treated as a profile press");
        check(ArturiaPadModes.managementPadCode(new byte[]{(byte) 0x90, 40, 127}) == 0,
                "keyboard-channel note was treated as a management pad");
        check(ArturiaPadModes.profileForCode(2) == null,
                "unsupported Synth Programmer mode was mapped silently");
        check(Sf2Profile.ANDROID_ORGAN_ID.equals(ArturiaPadModes.profileForCode(3)),
                "Android Full Live fallback should restore its Organ profile");
    }

    private static void verifiesOrganDefaultsAndPortableControlList() throws Exception {
        ArturiaControlState state = new ArturiaControlState();
        check(ArturiaControlState.CONTROL_COUNT == 18, "control count differs from C API");
        for (int index = 0; index < 18; index++) {
            check(state.valueAt(index) == (index == 16 ? 64 : 0),
                    "unexpected Organ startup control value at " + index);
        }
        check(!state.update(0xB0, 114, 64), "main encoder must not enter the CC cache");
        check(!state.update(0xB0, 115, 127), "encoder click must not enter the CC cache");
        check(!state.update(0xB1, 73, 88), "only channel-one CCs are cached");

        MockSynth synth = new MockSynth();
        RigHost organHost = new RigHost(synth, new MockRepository());
        check(organHost.masterVolume() == 127 && !organHost.masterMuted(),
                "master controls did not start at full volume and unmuted");
        Sf2Profile organ = organProfile();
        organHost.selectProfile(organ);
        check(organ.label.contains("not setBfree drawbars"),
                "Android organ mode must label its setBfree mapping difference");
        check(organ.mappingDescription.contains("Fader 1 controls channel volume"),
                "Android organ mapping description is missing");
        check(Arrays.equals(synth.messages.get(0), new byte[]{(byte) 0xB0, 7, 0}),
                "Organ startup fader value was not applied as layer volume");
        check(Arrays.equals(synth.messages.get(1), new byte[]{(byte) 0xB0, 91, 0}),
                "Organ startup knob value was not applied as layer reverb");
        organHost.close();

        MockSynth knobEightSynth = new MockSynth();
        RigHost knobEightHost = new RigHost(knobEightSynth, new MockRepository());
        knobEightHost.selectProfile(profile("knob-eight", new byte[]{8}));
        check(Arrays.equals(knobEightSynth.messages.get(1),
                new byte[]{(byte) 0xB0, 91, 64}),
                "knob 8's startup value was not replayed to its active layer");
        knobEightHost.close();
    }

    private static void verifiesSharedRoutePlan() {
        Sf2Profile profile = profile("worship-piano", new byte[]{1, 2, 3, 6, 8, 9});
        check(profile.routes.size() == 6, "wrong active route count");
        check(profile.routes.get(2).layer == 3, "route order was not preserved");
        check(profile.routes.get(2).mixerSlot == 3, "mixer slot should be compact");
        check(profile.routes.get(2).faderCc == 79 && profile.routes.get(2).knobCc == 76,
                "layer-three controls differ from portable plan");
        expectIllegalArgument(() -> SharedSf2Plan.build(new short[9], 1,
                new byte[]{3, 3}), "duplicate active layer accepted");
    }

    private static void verifiesProfileReplayMasterControlsAndLoadOnce() throws Exception {
        MockSynth synth = new MockSynth();
        MockRepository repository = new MockRepository();
        RigHost host = new RigHost(synth, repository);
        Sf2Profile first = profile("first", new byte[]{1, 2, 6});
        host.selectProfile(first);
        check(synth.messages.size() == 6, "profile startup should replay controls per active route");
        check(Arrays.equals(synth.messages.get(0), new byte[]{(byte) 0xB0, 7, 0}),
                "layer volume startup state was not replayed");
        check(Arrays.equals(synth.messages.get(5), new byte[]{(byte) 0xB2, 91, 0}),
                "active layer-six reverb was routed incorrectly");
        check(synth.configuredBanks[0] == 10 && synth.configuredPrograms[0] == 11,
                "resolved SF2 bank/program was not passed to the synth");

        synth.messages.clear();
        check(host.onMidiMessage(new byte[]{(byte) 0xB0, 75, 93}),
                "cached fader CC was not accepted");
        check(synth.messages.size() == 1 &&
                Arrays.equals(synth.messages.get(0), new byte[]{(byte) 0xB1, 7, 93}),
                "fader CC did not follow its output-volume route");

        synth.messages.clear();
        check(host.onMidiMessage(new byte[]{(byte) 0x90, 60, 100}),
                "note message was not routed");
        check(synth.messages.size() == 3, "note should be broadcast to active layers");
        check((synth.messages.get(2)[0] & 0xff) == 0x92,
                "note channel was not rewritten for the third mixer slot");

        synth.messages.clear();
        check(host.onMidiMessage(new byte[]{(byte) 0xB0, 114, 63}),
                "master-volume encoder was not handled");
        check(host.masterVolume() == 126 && synth.masterVolume == 126,
                "binary-offset encoder did not adjust master volume");
        check(synth.messages.isEmpty(), "master-volume encoder was forwarded to layer channels");
        check(host.onMidiMessage(new byte[]{(byte) 0xB0, 115, 127}),
                "master-mute click was not handled");
        check(host.masterMuted() && synth.masterMuted, "encoder click did not mute the master");
        host.onMidiMessage(new byte[]{(byte) 0xB0, 115, 127});
        check(host.masterMuted(), "a held encoder click toggled mute more than once");
        host.onMidiMessage(new byte[]{(byte) 0xB0, 115, 0});
        host.onMidiMessage(new byte[]{(byte) 0xB0, 115, 127});
        check(!host.masterMuted() && !synth.masterMuted,
                "encoder release did not re-arm master mute toggle");
        check(synth.messages.isEmpty(), "master controls were sent to individual layers");

        synth.messages.clear();
        host.selectProfile(profile("second", new byte[]{2, 4}));
        check(synth.messages.size() == 4, "profile switch did not replay controls");
        check(Arrays.equals(synth.messages.get(0), new byte[]{(byte) 0xB0, 7, 93}),
                "cached second-profile fader was not replayed");
        check(Arrays.equals(synth.messages.get(2), new byte[]{(byte) 0xB1, 7, 0}),
                "second route channel is wrong");
        check(synth.configureCount == 2, "profile changes were not configured");
        check(Arrays.equals(synth.configuredIds.get(0), synth.configuredIds.get(1)),
                "profile switch did not reuse loaded SoundFont IDs");
        check(synth.loadCountByPath.size() == first.soundFonts.size(),
                "profile switch loaded an unexpected SoundFont path");
        for (int loads : synth.loadCountByPath.values()) {
            check(loads == 1, "shared SoundFont loaded more than once across profile switch");
        }
        for (int count : repository.materializeCount.values()) {
            check(count == 1, "cached SoundFont file was materialized more than once");
        }
        check(synth.presetEnumerationCount == first.soundFonts.size(),
                "loaded SoundFont presets were re-enumerated on profile switch");
        host.close();
    }

    private static void verifiesPresetResolutionFailsClosed() throws Exception {
        MockSynth synth = new MockSynth();
        RigHost host = new RigHost(synth, new MockRepository());
        Sf2Profile missing = profileWithLabel("missing-preset", "No Such SF2 Preset",
                new byte[]{1});
        try {
            host.selectProfile(missing);
            throw new AssertionError("missing expected preset label was accepted");
        } catch (IllegalStateException expected) {
            check(expected.getMessage().contains("not found"),
                    "missing-preset failure was not diagnostic");
        }
        check(synth.configureCount == 0,
                "synth profile was configured despite the missing preset");
        host.close();
    }

    private static void verifiesPresetResolverInjection() throws Exception {
        MockSynth synth = new MockSynth();
        List<String> requestedLabels = new ArrayList<>();
        PresetResolver resolver = (expected, available) -> {
            requestedLabels.add(expected);
            return new Sf2Preset(expected, 37, 82);
        };
        RigHost host = new RigHost(synth, new MockRepository(), resolver);
        host.selectProfile(profileWithLabel("resolver-seam", "Expected Preset",
                new byte[]{1}));
        check(requestedLabels.size() == 1 &&
                "Expected Preset".equals(requestedLabels.get(0)),
                "preset resolver did not receive the catalog label");
        check(synth.configuredBanks[0] == 37 && synth.configuredPrograms[0] == 82,
                "resolved bank/program was not passed through the preset seam");
        host.close();
    }

    private static void verifiesMidiStreamRunningStatus() {
        List<byte[]> messages = new ArrayList<>();
        MidiStreamParser parser = new MidiStreamParser(messages::add);
        parser.accept(new byte[]{(byte) 0x90, 60, 100, 61, 0}, 0, 5);
        check(messages.size() == 2, "running-status MIDI stream was not split");
        check(Arrays.equals(messages.get(1), new byte[]{(byte) 0x90, 61, 0}),
                "running-status data did not retain its status byte");
    }

    private static Sf2Profile organProfile() {
        return Sf2Profile.androidOrgan("organ.sf2", "JF Gospel Organ",
                new byte[]{1, 2, 3, 4, 5, 6, 7, 8, 9}, 7, 91);
    }

    private static Sf2Profile profile(String id, byte[] activeLayers) {
        List<Sf2Profile.SoundFont> fonts = Arrays.asList(
                new Sf2Profile.SoundFont("a", "a.sf2", "A"),
                new Sf2Profile.SoundFont("b", "b.sf2", "B"),
                new Sf2Profile.SoundFont("c", "c.sf2", "C"),
                new Sf2Profile.SoundFont("d", "d.sf2", "D"),
                new Sf2Profile.SoundFont("e", "e.sf2", "E"),
                new Sf2Profile.SoundFont("f", "f.sf2", "F"));
        short[] indexes = {0, 1, 2, 3, 4, 5, 0, 1, 2};
        byte[] channels = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        return new Sf2Profile(id, id, fonts, indexes, activeLayers, channels, 7, 91,
                "Faders control layer volume; knobs control reverb.");
    }

    private static Sf2Profile profileWithLabel(String id, String presetLabel,
                                               byte[] activeLayers) {
        return profileWithLabel(id, presetLabel, activeLayers, id,
                "Faders control layer volume; knobs control reverb.");
    }

    private static Sf2Profile profileWithLabel(String id, String presetLabel,
                                               byte[] activeLayers, String profileLabel,
                                               String mappingDescription) {
        List<Sf2Profile.SoundFont> fonts = Arrays.asList(
                new Sf2Profile.SoundFont(id, "organ.sf2", presetLabel));
        short[] indexes = new short[9];
        byte[] channels = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        return new Sf2Profile(id, profileLabel, fonts, indexes, activeLayers, channels,
                7, 91, mappingDescription);
    }

    private static void expectIllegalArgument(Runnable action, String message) {
        try {
            action.run();
        } catch (IllegalArgumentException expected) {
            return;
        }
        throw new AssertionError(message);
    }

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static final class MockRepository implements SoundFontRepository {
        private final Map<String, Integer> materializeCount = new HashMap<>();

        @Override
        public String materialize(String relativePath) {
            materializeCount.put(relativePath,
                    materializeCount.containsKey(relativePath)
                            ? materializeCount.get(relativePath) + 1 : 1);
            return "/mock/" + relativePath;
        }
    }

    private static final class MockSynth implements SynthEngine {
        private final List<byte[]> messages = new ArrayList<>();
        private final Map<String, Integer> loadCountByPath = new HashMap<>();
        private int nextSoundFontId = 20;
        private int configureCount;
        private int presetEnumerationCount;
        private int[] configuredBanks;
        private int[] configuredPrograms;
        private final List<int[]> configuredIds = new ArrayList<>();
        private int masterVolume;
        private boolean masterMuted;

        @Override
        public int loadSoundFont(String path) {
            loadCountByPath.put(path,
                    loadCountByPath.containsKey(path) ? loadCountByPath.get(path) + 1 : 1);
            return nextSoundFontId++;
        }

        @Override
        public List<Sf2Preset> listPresets(int soundFontId) {
            presetEnumerationCount++;
            List<Sf2Preset> presets = new ArrayList<>();
            for (String label : Arrays.asList("A", "B", "C", "D", "E", "F")) {
                presets.add(new Sf2Preset(label, 10, 11));
            }
            presets.add(new Sf2Preset("JF Gospel Organ", 7, 12));
            return presets;
        }

        @Override
        public void configureProfile(Sf2Profile profile, int[] soundFontIds,
                                     int[] presetBanks, int[] presetPrograms) {
            check(soundFontIds.length == profile.soundFonts.size(),
                    "mock SoundFont ID list mismatch");
            configureCount++;
            configuredIds.add(soundFontIds.clone());
            configuredBanks = presetBanks.clone();
            configuredPrograms = presetPrograms.clone();
        }

        @Override
        public void sendMidi(byte[] message) {
            messages.add(message.clone());
        }

        @Override
        public void observeControllerMessage(byte[] message) {
        }

        @Override
        public void setMasterVolume(int volume) {
            masterVolume = volume;
        }

        @Override
        public void setMasterMuted(boolean muted) {
            masterMuted = muted;
        }

        @Override
        public void close() {
        }
    }
}
