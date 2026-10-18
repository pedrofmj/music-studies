package org.musicrig.core;

import java.util.List;

public interface SynthEngine extends AutoCloseable {
    int loadSoundFont(String materializedPath) throws Exception;

    List<Sf2Preset> listPresets(int soundFontId) throws Exception;

    void configureProfile(Sf2Profile profile, int[] soundFontIds,
                          int[] presetBanks, int[] presetPrograms) throws Exception;

    void sendMidi(byte[] message);

    void observeControllerMessage(byte[] message);

    void setMasterVolume(int volume);

    void setMasterMuted(boolean muted);

    @Override
    void close();
}
