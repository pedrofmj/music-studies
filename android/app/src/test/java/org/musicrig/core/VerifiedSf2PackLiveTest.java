package org.musicrig.core;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.Locale;

public final class VerifiedSf2PackLiveTest {
    private static final String SMALL_ASSET_PATH =
            "02_SoundFonts/05_Strings_and_Orchestral/Dark Violins.sf2";
    private static final String EXPECTED_SHA256 =
            "e9d682fd69f1d08d9f85b93ac5b054352bcff2595dfdabe13d9903174ae45e6a";
    private static final long EXPECTED_SIZE = 496276L;

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 1) {
            throw new IllegalArgumentException("Pass the Android pack HTTPS base URL");
        }
        File temporary = File.createTempFile("sf2-pack-live-", "");
        if (!temporary.delete() || !temporary.mkdirs()) {
            throw new IllegalStateException("Could not create live-check cache directory");
        }
        try {
            File asset = new File(new VerifiedSf2Pack(arguments[0], temporary)
                    .materialize(SMALL_ASSET_PATH));
            if (asset.length() != EXPECTED_SIZE || !EXPECTED_SHA256.equals(sha256(asset))) {
                throw new AssertionError("Live SF2 asset differs from the published pack index");
            }
            System.out.println("Live Android pack index and SHA-256-verified download passed");
        } finally {
            deleteTree(temporary);
        }
    }

    private static String sha256(File file) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                digest.update(buffer, 0, count);
            }
        }
        StringBuilder result = new StringBuilder(64);
        for (byte value : digest.digest()) {
            result.append(String.format(Locale.ROOT, "%02x", value & 0xff));
        }
        return result.toString();
    }

    private static void deleteTree(File path) {
        File[] children = path.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteTree(child);
            }
        }
        path.delete();
    }
}
