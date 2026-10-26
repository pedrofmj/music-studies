package org.musicrig.android;

import android.content.res.AssetManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.musicrig.core.Sf2Profile;
import org.musicrig.core.SharedSf2Plan;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class ProfileCatalog {
    public static final String ORGAN_PROFILE_ID = Sf2Profile.ANDROID_ORGAN_ID;

    private final JSONObject patchCatalog;
    private final JSONObject routing;
    private final JSONObject androidOverrides;
    private final List<String> genres;

    private ProfileCatalog(JSONObject patchCatalog, JSONObject routing,
                           JSONObject androidOverrides) throws JSONException {
        this.patchCatalog = patchCatalog;
        this.routing = routing;
        this.androidOverrides = androidOverrides;
        JSONObject genreMap = patchCatalog.getJSONObject("genres");
        genres = new ArrayList<>();
        for (java.util.Iterator<String> keys = genreMap.keys(); keys.hasNext();) {
            genres.add(keys.next());
        }
    }

    public static ProfileCatalog load(AssetManager assets) throws IOException, JSONException {
        JSONObject patches;
        JSONObject routes;
        JSONObject overrides;
        try (InputStream input = assets.open("arturia-genre-patches.json")) {
            patches = new JSONObject(readUtf8(input));
        }
        try (InputStream input = assets.open("arturia-shared-sf2-routing.json")) {
            routes = new JSONObject(readUtf8(input));
        }
        try (InputStream input = assets.open("arturia-sf2-assets.android.json")) {
            overrides = new JSONObject(readUtf8(input));
        }
        if (!"music-studies/arturia-genre-patches/v1".equals(
                patches.getString("schema")) ||
                !"music-studies/arturia-shared-sf2-routing/v1".equals(
                        routes.getString("schema")) ||
                !"arturia-genre-patches.json".equals(routes.getString("source_catalog")) ||
                !SharedSf2Plan.matchesControlMaps(
                        integerArray(routes.getJSONArray("fader_ccs")),
                        integerArray(routes.getJSONArray("knob_ccs")))) {
            throw new JSONException("Unsupported Arturia catalog schema");
        }
        validateAndroidOverrides(patches, overrides);
        return new ProfileCatalog(patches, routes, overrides);
    }

    public List<String> genres() {
        return new ArrayList<>(genres);
    }

    public List<ProfileOption> profileOptions() {
        List<ProfileOption> options = new ArrayList<>();
        options.add(new ProfileOption(ORGAN_PROFILE_ID,
                Sf2Profile.ANDROID_ORGAN_LABEL));
        for (String genre : genres) {
            options.add(new ProfileOption(genre, genre));
        }
        return options;
    }

    public Sf2Profile profile(String id) throws JSONException {
        if (ORGAN_PROFILE_ID.equals(id)) {
            return organProfile();
        }
        String genre = id;
        JSONArray patchKeys = patchCatalog.getJSONObject("genres").getJSONArray(genre);
        JSONArray activeJson = routing.getJSONObject("active_layers").getJSONArray(genre);
        JSONArray channelJson = routing.getJSONArray("map_channels");
        JSONObject patches = patchCatalog.getJSONObject("patches");
        JSONObject substitutions = routing.getJSONObject("sf2_substitutions");
        List<Integer> active = new ArrayList<>();
        List<Sf2Profile.SoundFont> fonts = new ArrayList<>();
        Map<String, Integer> pathIndexes = new LinkedHashMap<>();
        short[] patchIndexes = new short[SharedSf2Plan.LAYER_COUNT];
        byte[] activeLayers = new byte[activeJson.length()];
        byte[] mapChannels = new byte[channelJson.length()];

        for (int index = 0; index < activeJson.length(); index++) {
            int layer = activeJson.getInt(index);
            if (layer < 1 || layer > patchKeys.length()) {
                throw new JSONException("Active layer is outside the genre patch list");
            }
            active.add(layer);
            activeLayers[index] = (byte) layer;
        }
        for (int index = 0; index < channelJson.length(); index++) {
            mapChannels[index] = (byte) channelJson.getInt(index);
        }

        for (int layer = 1; layer <= SharedSf2Plan.LAYER_COUNT; layer++) {
            String patchId = patchKeys.getString(layer - 1);
            JSONObject patch = patches.getJSONObject(patchId);
            if (!"sf2".equals(patch.getString("engine"))) {
                patchId = substitutions.getString(patchId);
                patch = patches.getJSONObject(patchId);
            }
            if (!"sf2".equals(patch.getString("engine"))) {
                throw new JSONException("SF2 substitution does not resolve to an SF2 patch");
            }
            String path = runtimePath(patchId, patch);
            String label = runtimeLabel(patchId, patch);
            if (active.contains(layer)) {
                Integer patchIndex = pathIndexes.get(path);
                if (patchIndex == null) {
                    patchIndex = fonts.size();
                    pathIndexes.put(path, patchIndex);
                    fonts.add(new Sf2Profile.SoundFont(patchId, path, label));
                }
                patchIndexes[layer - 1] = patchIndex.shortValue();
            }
        }
        if (fonts.isEmpty()) {
            throw new JSONException("Genre has no active SF2 patches");
        }
        return new Sf2Profile(genre, genre, fonts, patchIndexes, activeLayers,
                mapChannels, routing.getInt("volume_output_cc"),
                routing.getInt("reverb_output_cc"),
                "Active SF2 layers: faders control layer volume and knobs control reverb.");
    }

    private Sf2Profile organProfile() throws JSONException {
        String patchId = "organ-gospel";
        JSONObject patch = patchCatalog.getJSONObject("patches").getJSONObject(patchId);
        if (!"sf2".equals(patch.getString("engine"))) {
            throw new JSONException("Android Organ profile requires an SF2 catalog patch");
        }
        JSONArray channelJson = routing.getJSONArray("map_channels");
        byte[] mapChannels = new byte[channelJson.length()];
        for (int index = 0; index < channelJson.length(); index++) {
            mapChannels[index] = (byte) channelJson.getInt(index);
        }
        return Sf2Profile.androidOrgan(runtimePath(patchId, patch),
                runtimeLabel(patchId, patch),
                mapChannels, routing.getInt("volume_output_cc"),
                routing.getInt("reverb_output_cc"));
    }

    private String runtimePath(String patchId, JSONObject catalogPatch) throws JSONException {
        JSONObject override = androidOverrides.getJSONObject("patches").optJSONObject(patchId);
        return override == null ? catalogPatch.getString("path") :
                override.getString("logical_path");
    }

    private String runtimeLabel(String patchId, JSONObject catalogPatch) throws JSONException {
        JSONObject override = androidOverrides.getJSONObject("patches").optJSONObject(patchId);
        return override == null ? catalogPatch.getString("label") : override.getString("label");
    }

    private static void validateAndroidOverrides(JSONObject catalog, JSONObject overrides)
            throws JSONException {
        if (!"music-studies/arturia-platform-sf2-overrides/v1".equals(
                overrides.getString("schema")) ||
                !"android".equals(overrides.getString("platform")) ||
                !"arturia-genre-patches.json".equals(overrides.getString("source_catalog"))) {
            throw new JSONException("Unsupported Android SF2 override catalog");
        }
        JSONObject catalogPatches = catalog.getJSONObject("patches");
        JSONObject overridePatches = overrides.getJSONObject("patches");
        for (java.util.Iterator<String> keys = overridePatches.keys(); keys.hasNext();) {
            String patchId = keys.next();
            if (!catalogPatches.has(patchId)) {
                throw new JSONException("Android SF2 override references an unknown patch");
            }
            JSONObject override = overridePatches.getJSONObject(patchId);
            String path = override.getString("logical_path");
            String label = override.getString("label");
            if (!isSafeCatalogPath(path) || label.isEmpty()) {
                throw new JSONException("Android SF2 override has an invalid path or label");
            }
        }
    }

    private static boolean isSafeCatalogPath(String path) {
        if (path.isEmpty() || path.startsWith("/") || path.indexOf('\\') >= 0) {
            return false;
        }
        for (String component : path.split("/", -1)) {
            if (component.isEmpty() || ".".equals(component) || "..".equals(component)) {
                return false;
            }
        }
        return true;
    }

    private static String readUtf8(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = input.read(buffer)) != -1) {
            output.write(buffer, 0, count);
        }
        return new String(output.toByteArray(), StandardCharsets.UTF_8);
    }

    private static int[] integerArray(JSONArray values) throws JSONException {
        int[] result = new int[values.length()];
        for (int index = 0; index < values.length(); index++) {
            result[index] = values.getInt(index);
        }
        return result;
    }

    public static final class ProfileOption {
        public final String id;
        public final String label;

        ProfileOption(String id, String label) {
            this.id = id;
            this.label = label;
        }

        @Override
        public String toString() {
            return label;
        }
    }
}
