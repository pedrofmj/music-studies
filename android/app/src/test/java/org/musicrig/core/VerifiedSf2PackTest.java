package org.musicrig.core;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashMap;
import java.util.Map;

public final class VerifiedSf2PackTest {
    private static final String BASE_URL = "https://pack.test/android/";

    public static void main(String[] arguments) throws Exception {
        verifiesLazyDownloadDeduplicationAndPrefetch();
        rejectsCorruptAssetsWithoutPublishingPartialFiles();
        rejectsUnsafeIndexPaths();
    }

    private static void verifiesLazyDownloadDeduplicationAndPrefetch() throws Exception {
        byte[] first = "sf2-one".getBytes(StandardCharsets.UTF_8);
        byte[] second = "sf2-two".getBytes(StandardCharsets.UTF_8);
        String firstHash = sha256(first);
        String secondHash = sha256(second);
        FakeSource source = new FakeSource();
        source.add("asset-index.tsv", index(
                row("strings/Cinemático.sf2", firstHash, first.length),
                row("strings/alias.sf2", firstHash, first.length),
                row("piano/soft grand.sf2", secondHash, second.length)));
        source.add("assets/" + firstHash + ".sf2", first);
        source.add("assets/" + secondHash + ".sf2", second);
        File cache = newCacheDirectory();
        try {
            VerifiedSf2Pack pack = new VerifiedSf2Pack(BASE_URL, cache, source);
            File materialized = new File(pack.materialize("strings/Cinemático.sf2"));
            check(materialized.isFile() && materialized.length() == first.length,
                    "lazy SF2 download did not materialize the indexed file");
            pack.materialize("strings/alias.sf2");
            check(source.count("assets/" + firstHash + ".sf2") == 1,
                    "identical catalog paths downloaded the same content more than once");

            final int[] progress = {0};
            int assetCount = pack.prefetchAll((completed, total) -> {
                check(total == 2, "prefetch did not deduplicate shared asset content");
                progress[0] = completed;
            });
            check(assetCount == 2 && progress[0] == 2,
                    "prefetch did not finish every unique pack asset");
            check(source.count("assets/" + firstHash + ".sf2") == 1 &&
                            source.count("assets/" + secondHash + ".sf2") == 1,
                    "prefetch did not reuse the already cached asset");

            VerifiedSf2Pack offlinePack = new VerifiedSf2Pack(BASE_URL, cache, url -> {
                throw new IOException("network should not be needed for a verified cache hit");
            });
            check(new File(offlinePack.materialize("piano/soft grand.sf2")).isFile(),
                    "cached index/assets were not usable offline");
        } finally {
            deleteTree(cache);
        }
    }

    private static void rejectsCorruptAssetsWithoutPublishingPartialFiles() throws Exception {
        byte[] expected = "valid-sf2".getBytes(StandardCharsets.UTF_8);
        String hash = sha256(expected);
        FakeSource source = new FakeSource();
        source.add("asset-index.tsv", index(row("piano/test.sf2", hash, expected.length)));
        source.add("assets/" + hash + ".sf2", "corrupt".getBytes(StandardCharsets.UTF_8));
        File cache = newCacheDirectory();
        try {
            expectIOException(() -> new VerifiedSf2Pack(BASE_URL, cache, source)
                    .materialize("piano/test.sf2"));
            check(!new File(cache, hash + ".sf2").exists(),
                    "hash-mismatched SF2 was published in the cache");
            File[] files = cache.listFiles();
            for (File file : files) {
                check(!file.getName().endsWith(".part"),
                        "failed download left a partial file behind");
            }
        } finally {
            deleteTree(cache);
        }
    }

    private static void rejectsUnsafeIndexPaths() throws Exception {
        FakeSource source = new FakeSource();
        String hash = sha256("sf2".getBytes(StandardCharsets.UTF_8));
        source.add("asset-index.tsv", index(row("../outside.sf2", hash, 3)));
        File cache = newCacheDirectory();
        try {
            expectIOException(() -> new VerifiedSf2Pack(BASE_URL, cache, source)
                    .materialize("../outside.sf2"));
            check(source.count("assets/" + hash + ".sf2") == 0,
                    "unsafe index path caused an asset request");
        } finally {
            deleteTree(cache);
        }
    }

    private static String index(String... rows) {
        StringBuilder result = new StringBuilder(
                "music-studies/arturia-sf2-pack-index/v1\n" +
                        "catalog_path\tasset_path\tsha256\tsize_bytes\n");
        for (String row : rows) {
            result.append(row).append('\n');
        }
        return result.toString();
    }

    private static String row(String catalogPath, String hash, long size) {
        return catalogPath + "\tassets/" + hash + ".sf2\t" + hash + "\t" + size;
    }

    private static String sha256(byte[] content) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(content);
        StringBuilder result = new StringBuilder(digest.length * 2);
        for (byte value : digest) {
            result.append(String.format(java.util.Locale.ROOT, "%02x", value & 0xff));
        }
        return result.toString();
    }

    private static File newCacheDirectory() throws IOException {
        File directory = File.createTempFile("sf2-pack-test-", "");
        if (!directory.delete() || !directory.mkdirs()) {
            throw new IOException("Could not create test cache directory");
        }
        return directory;
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

    private static void expectIOException(ThrowingAction action) throws Exception {
        try {
            action.run();
        } catch (IOException expected) {
            return;
        }
        throw new AssertionError("invalid SF2 pack data was accepted");
    }

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private interface ThrowingAction {
        void run() throws Exception;
    }

    private static final class FakeSource implements VerifiedSf2Pack.StreamSource {
        private final Map<String, byte[]> responses = new HashMap<>();
        private final Map<String, Integer> requests = new HashMap<>();

        void add(String path, byte[] content) {
            responses.put(BASE_URL + path, content);
        }

        void add(String path, String content) {
            add(path, content.getBytes(StandardCharsets.UTF_8));
        }

        int count(String path) {
            Integer count = requests.get(BASE_URL + path);
            return count == null ? 0 : count;
        }

        @Override
        public InputStream open(String url) throws IOException {
            requests.put(url, requests.containsKey(url) ? requests.get(url) + 1 : 1);
            byte[] content = responses.get(url);
            if (content == null) {
                throw new IOException("unexpected URL: " + url);
            }
            return new ByteArrayInputStream(content);
        }
    }
}
