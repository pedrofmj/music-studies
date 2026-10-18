package org.musicrig.core;

import java.util.List;

public interface PresetResolver {
    Sf2Preset resolve(String expectedName, List<Sf2Preset> availablePresets);
}
