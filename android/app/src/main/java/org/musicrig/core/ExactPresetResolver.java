package org.musicrig.core;

import java.util.List;

public final class ExactPresetResolver implements PresetResolver {
    @Override
    public Sf2Preset resolve(String expectedName, List<Sf2Preset> availablePresets) {
        Sf2Preset match = null;
        for (Sf2Preset preset : availablePresets) {
            if (expectedName.equals(preset.name)) {
                if (match != null) {
                    throw new IllegalStateException("SF2 preset label is ambiguous: " +
                            expectedName);
                }
                match = preset;
            }
        }
        if (match == null) {
            throw new IllegalStateException("SF2 preset label not found: " + expectedName);
        }
        return match;
    }
}
