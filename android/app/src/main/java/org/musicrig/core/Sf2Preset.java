package org.musicrig.core;

public final class Sf2Preset {
    public final String name;
    public final int bank;
    public final int program;

    public Sf2Preset(String name, int bank, int program) {
        if (name == null || bank < 0 || bank > 16383 || program < 0 || program > 127) {
            throw new IllegalArgumentException("Invalid SF2 preset");
        }
        this.name = name;
        this.bank = bank;
        this.program = program;
    }
}
