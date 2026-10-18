package org.musicrig.core;

public final class ArturiaPadModes {
    private static final int[] NOTES = {
            40, 41, 42, 43, 36, 37, 38, 39,
            48, 49, 50, 51, 44, 45, 46, 47
    };
    private static final String[] GENRES = {
            "worship-piano", "gospel-keys", "ambient-worship", "jazz-keys",
            "soul-rnb", "cinematic-strings", "orchestral", "brass-winds",
            "synthwave", "retro-keys", "intimate-pads", "praise-leads",
            "acoustic-worship"
    };

    private ArturiaPadModes() {
    }

    public static int managementPadCode(byte[] message) {
        if (message == null || message.length != 3) {
            return 0;
        }
        int status = message[0] & 0xff;
        int type = status & 0xf0;
        if ((status & 0x0f) != 9 || (type != 0x80 && type != 0x90)) {
            return 0;
        }
        int note = message[1] & 0x7f;
        for (int index = 0; index < NOTES.length; index++) {
            if (NOTES[index] == note) {
                return index + 1;
            }
        }
        return 0;
    }

    public static boolean isPress(byte[] message) {
        return message != null && message.length == 3 &&
                (message[0] & 0xff) == 0x99 && (message[2] & 0x7f) != 0 &&
                managementPadCode(message) != 0;
    }

    public static String profileForCode(int code) {
        if (code == 1 || code == 3) {
            return Sf2Profile.ANDROID_ORGAN_ID;
        }
        if (code >= 4 && code <= 16) {
            return GENRES[code - 4];
        }
        return null;
    }
}
