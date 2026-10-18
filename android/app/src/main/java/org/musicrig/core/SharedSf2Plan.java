package org.musicrig.core;

import java.util.ArrayList;
import java.util.List;

public final class SharedSf2Plan {
    public static final int LAYER_COUNT = 9;
    private static final int[] FADER_CCS = {73, 75, 79, 72, 80, 81, 82, 83, 85};
    private static final int[] KNOB_CCS = {74, 71, 76, 77, 93, 18, 19, 16, 17};

    private SharedSf2Plan() {
    }

    public static List<Route> build(short[] patchIndexes, int patchCount,
                                    byte[] activeLayers) {
        if (patchIndexes == null || patchIndexes.length != LAYER_COUNT ||
                activeLayers == null || activeLayers.length == 0 ||
                activeLayers.length > LAYER_COUNT || patchCount <= 0) {
            throw new IllegalArgumentException("Invalid shared SF2 route inputs");
        }
        for (short patchIndex : patchIndexes) {
            if (patchIndex < 0 || patchIndex >= patchCount) {
                throw new IllegalArgumentException("Patch index is outside the SF2 list");
            }
        }

        boolean[] active = new boolean[LAYER_COUNT];
        List<Route> routes = new ArrayList<>();
        for (int index = 0; index < activeLayers.length; index++) {
            int layer = activeLayers[index] & 0xff;
            if (layer < 1 || layer > LAYER_COUNT || active[layer - 1]) {
                throw new IllegalArgumentException("Invalid or duplicate active layer");
            }
            active[layer - 1] = true;
            routes.add(new Route(patchIndexes[layer - 1], layer, index + 1,
                    FADER_CCS[layer - 1], KNOB_CCS[layer - 1]));
        }
        return routes;
    }

    public static boolean matchesControlMaps(int[] faderCcs, int[] knobCcs) {
        if (faderCcs == null || knobCcs == null ||
                faderCcs.length != LAYER_COUNT || knobCcs.length != LAYER_COUNT) {
            return false;
        }
        for (int index = 0; index < LAYER_COUNT; index++) {
            if (faderCcs[index] != FADER_CCS[index] || knobCcs[index] != KNOB_CCS[index]) {
                return false;
            }
        }
        return true;
    }

    public static final class Route {
        public final int patchIndex;
        public final int layer;
        public final int mixerSlot;
        public final int faderCc;
        public final int knobCc;

        Route(int patchIndex, int layer, int mixerSlot, int faderCc, int knobCc) {
            this.patchIndex = patchIndex;
            this.layer = layer;
            this.mixerSlot = mixerSlot;
            this.faderCc = faderCc;
            this.knobCc = knobCc;
        }
    }
}
