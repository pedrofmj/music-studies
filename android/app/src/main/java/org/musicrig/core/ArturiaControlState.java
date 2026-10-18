package org.musicrig.core;

public final class ArturiaControlState {
    public static final int CONTROL_COUNT = 18;
    private static final int[] CONTROL_CCS = {
            73, 75, 79, 72, 80, 81, 82, 83, 85,
            74, 71, 76, 77, 93, 18, 19, 16, 17
    };

    private final int[] values = new int[CONTROL_COUNT];

    public ArturiaControlState() {
        values[16] = 64;
    }

    public boolean update(int status, int controller, int value) {
        if (status != 0xB0 || value < 0 || value > 127) {
            return false;
        }
        int index = indexOf(controller);
        if (index < 0) {
            return false;
        }
        values[index] = value;
        return true;
    }

    public int indexOf(int controller) {
        for (int index = 0; index < CONTROL_CCS.length; index++) {
            if (CONTROL_CCS[index] == controller) {
                return index;
            }
        }
        return -1;
    }

    public byte[] message(int index, int channel) {
        if (index < 0 || index >= CONTROL_CCS.length || channel < 1 || channel > 16) {
            throw new IllegalArgumentException("Invalid control index or MIDI channel");
        }
        return new byte[]{(byte) (0xB0 | (channel - 1)),
                (byte) CONTROL_CCS[index], (byte) values[index]};
    }

    public int valueAt(int index) {
        return values[index];
    }
}
