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
    private final List<String> genres;

    private ProfileCatalog(JSONObject patchCatalog, JSONObject routing) throws JSONException {
        this.patchCatalog = patchCatalog;
        this.routing = routing;
        JSONObject genreMap = patchCatalog.getJSONObject("genres");
        genres = new ArrayList<>();
        for (java.util.Iterator<String> keys = genreMap.keys(); keys.hasNext();) {
            genres.add(keys.next());
        }
    }

    public static ProfileCatalog load(AssetManager assets) throws IOException, JSONException {
        JSONObject patches;
        JSONObject routes;
        try (InputStream input = assets.open("arturia-genre-patches.json")) {
            patches = new JSONObject(readUtf8(input));
        }
        try (InputStream input = assets.open("arturia-shared-sf2-routing.json")) {
            routes = new JSONObject(readUtf8(input));
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
        return new ProfileCatalog(patches, routes);
    }

    public List<String> genres() {
        return new ArrayList<>(genres);
    }

    public List<Sf2Profile> genreProfiles() throws JSONException {
        List<Sf2Profile> profiles = new ArrayList<>();
        for (String genre : genres) {
            profiles.add(profile(genre));
        }
        return profiles;
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
            String path = patch.getString("path");
            if (active.contains(layer)) {
                Integer patchIndex = pathIndexes.get(path);
                if (patchIndex == null) {
                    patchIndex = fonts.size();
                    pathIndexes.put(path, patchIndex);
                    fonts.add(new Sf2Profile.SoundFont(patchId, path,
                            patch.getString("label")));
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
        JSONObject patch = patchCatalog.getJSONObject("patches").getJSONObject("organ-gospel");
        if (!"sf2".equals(patch.getString("engine"))) {
            throw new JSONException("Android Organ profile requires an SF2 catalog patch");
        }
        JSONArray channelJson = routing.getJSONArray("map_channels");
        byte[] mapChannels = new byte[channelJson.length()];
        for (int index = 0; index < channelJson.length(); index++) {
            mapChannels[index] = (byte) channelJson.getInt(index);
        }
        return Sf2Profile.androidOrgan(patch.getString("path"), patch.getString("label"),
                mapChannels, routing.getInt("volume_output_cc"),
                routing.getInt("reverb_output_cc"));
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
