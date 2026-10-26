package org.musicrig.core;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import javax.net.ssl.HttpsURLConnection;

public final class VerifiedSf2Pack {
    private static final String INDEX_SCHEMA = "music-studies/arturia-sf2-pack-index/v1";
    private static final String INDEX_HEADER = "catalog_path\tasset_path\tsha256\tsize_bytes";
    private static final int MAX_INDEX_BYTES = 2 * 1024 * 1024;
    private static final int MAX_ASSETS = 256;
    private static final long MAX_ASSET_BYTES = 512L * 1024L * 1024L;
    private static final int BUFFER_SIZE = 64 * 1024;

    public interface StreamSource {
        InputStream open(String url) throws IOException;
    }

    public interface ProgressListener {
        void onAssetReady(int completedAssets, int totalAssets);
    }

    private final String baseUrl;
    private final File cacheDirectory;
    private final File indexFile;
    private final StreamSource source;
    private final Set<String> verifiedHashes = new HashSet<>();
    private Map<String, Asset> assetsByCatalogPath;
    private Map<String, Asset> uniqueAssets;

    public VerifiedSf2Pack(String baseUrl, File cacheDirectory) {
        this(baseUrl, cacheDirectory, VerifiedSf2Pack::openHttps);
    }

    public VerifiedSf2Pack(String baseUrl, File cacheDirectory, StreamSource source) {
        if (baseUrl == null || !baseUrl.startsWith("https://") ||
                !baseUrl.endsWith("/") || baseUrl.indexOf('?') >= 0 ||
                baseUrl.indexOf('#') >= 0 || cacheDirectory == null || source == null) {
            throw new IllegalArgumentException("HTTPS pack URL, cache directory, and source are required");
        }
        this.baseUrl = baseUrl;
        this.cacheDirectory = cacheDirectory;
        this.indexFile = new File(cacheDirectory, "asset-index.tsv");
        this.source = source;
    }

    public synchronized String materialize(String catalogPath) throws Exception {
        loadIndex();
        Asset asset = assetsByCatalogPath.get(catalogPath);
        if (asset == null) {
            throw new IOException("SF2 path is not in the Android pack index: " + catalogPath);
        }
        return ensureDownloaded(asset).getAbsolutePath();
    }

    public synchronized int prefetchAll(ProgressListener listener) throws Exception {
        loadIndex();
        int total = uniqueAssets.size();
        int completed = 0;
        for (Asset asset : uniqueAssets.values()) {
            ensureDownloaded(asset);
            completed++;
            if (listener != null) {
                listener.onAssetReady(completed, total);
            }
        }
        return total;
    }

    private void loadIndex() throws Exception {
        if (assetsByCatalogPath != null) {
            return;
        }
        ensureCacheDirectory();
        if (indexFile.isFile()) {
            try {
                byte[] bytes;
                try (InputStream input = new FileInputStream(indexFile)) {
                    bytes = readLimited(input, MAX_INDEX_BYTES);
                }
                installIndex(parseIndex(new String(bytes, StandardCharsets.UTF_8)));
                return;
            } catch (Exception invalidCache) {
                if (!indexFile.delete() && indexFile.exists()) {
                    throw new IOException("Could not discard the invalid cached SF2 index",
                            invalidCache);
                }
            }
        }

        byte[] bytes;
        try (InputStream input = source.open(baseUrl + "asset-index.tsv")) {
            if (input == null) {
                throw new IOException("SF2 server returned no index stream");
            }
            bytes = readLimited(input, MAX_INDEX_BYTES);
        }
        Map<String, Asset> parsed = parseIndex(new String(bytes, StandardCharsets.UTF_8));
        publishIndex(bytes);
        installIndex(parsed);
    }

    private void installIndex(Map<String, Asset> parsed) {
        assetsByCatalogPath = new HashMap<>();
        uniqueAssets = new LinkedHashMap<>();
        for (Map.Entry<String, Asset> entry : parsed.entrySet()) {
            assetsByCatalogPath.put(entry.getKey(), entry.getValue());
            uniqueAssets.put(entry.getValue().sha256, entry.getValue());
        }
    }

    private Map<String, Asset> parseIndex(String text) throws IOException {
        String[] lines = text.split("\n");
        if (lines.length < 3 || !INDEX_SCHEMA.equals(stripCarriageReturn(lines[0])) ||
                !INDEX_HEADER.equals(stripCarriageReturn(lines[1])) ||
                lines.length - 2 > MAX_ASSETS) {
            throw new IOException("Unsupported or oversized Android SF2 asset index");
        }
        Map<String, Asset> assets = new LinkedHashMap<>();
        Map<String, Asset> byHash = new HashMap<>();
        for (int index = 2; index < lines.length; index++) {
            String line = stripCarriageReturn(lines[index]);
            String[] fields = line.split("\t", -1);
            if (fields.length != 4 || !isSafeRelativePath(fields[0]) ||
                    !isSha256(fields[2])) {
                throw new IOException("Invalid row in Android SF2 asset index at line " +
                        (index + 1));
            }
            String expectedAssetPath = "assets/" + fields[2] + ".sf2";
            if (!expectedAssetPath.equals(fields[1])) {
                throw new IOException("SF2 index asset path does not match its digest");
            }
            final long size;
            try {
                size = Long.parseLong(fields[3]);
            } catch (NumberFormatException failure) {
                throw new IOException("Invalid SF2 size in asset index", failure);
            }
            if (size <= 0 || size > MAX_ASSET_BYTES || assets.containsKey(fields[0])) {
                throw new IOException("Invalid or duplicate SF2 index entry: " + fields[0]);
            }

            Asset asset = byHash.get(fields[2]);
            if (asset == null) {
                asset = new Asset(fields[1], fields[2], size);
                byHash.put(fields[2], asset);
            } else if (asset.sizeBytes != size || !asset.path.equals(fields[1])) {
                throw new IOException("Conflicting metadata for a shared SF2 asset");
            }
            assets.put(fields[0], asset);
        }
        if (assets.isEmpty()) {
            throw new IOException("Android SF2 asset index is empty");
        }
        return assets;
    }

    private File ensureDownloaded(Asset asset) throws Exception {
        File destination = new File(cacheDirectory, asset.sha256 + ".sf2");
        if (destination.isFile()) {
            if (verifiedHashes.contains(asset.sha256) || verifyFile(destination, asset)) {
                verifiedHashes.add(asset.sha256);
                return destination;
            }
            if (!destination.delete()) {
                throw new IOException("Could not remove a corrupt cached SF2 asset");
            }
        }
        if (cacheDirectory.getUsableSpace() < asset.sizeBytes) {
            throw new IOException("Not enough free storage for SF2 asset " + asset.path);
        }

        File temporary = File.createTempFile("sf2-", ".part", cacheDirectory);
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            long received = 0;
            try (InputStream input = source.open(baseUrl + asset.path);
                 FileOutputStream output = new FileOutputStream(temporary)) {
                if (input == null) {
                    throw new IOException("SF2 server returned no asset stream");
                }
                byte[] buffer = new byte[BUFFER_SIZE];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    received += count;
                    if (received > asset.sizeBytes) {
                        throw new IOException("SF2 download is larger than its indexed size");
                    }
                    digest.update(buffer, 0, count);
                    output.write(buffer, 0, count);
                }
                output.getFD().sync();
            }
            if (received != asset.sizeBytes ||
                    !toHex(digest.digest()).equals(asset.sha256)) {
                throw new IOException("SF2 download failed its size or SHA-256 check");
            }
            if (!temporary.renameTo(destination)) {
                throw new IOException("Could not atomically publish the verified SF2 asset");
            }
            verifiedHashes.add(asset.sha256);
            return destination;
        } finally {
            if (temporary.exists()) {
                temporary.delete();
            }
        }
    }

    private boolean verifyFile(File file, Asset asset) throws Exception {
        if (file.length() != asset.sizeBytes) {
            return false;
        }
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        try (InputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int count;
            while ((count = input.read(buffer)) != -1) {
                digest.update(buffer, 0, count);
            }
        }
        return toHex(digest.digest()).equals(asset.sha256);
    }

    private void publishIndex(byte[] bytes) throws IOException {
        File temporary = File.createTempFile("index-", ".part", cacheDirectory);
        try {
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                output.write(bytes);
                output.getFD().sync();
            }
            if (indexFile.exists() && !indexFile.delete()) {
                throw new IOException("Could not replace cached SF2 index");
            }
            if (!temporary.renameTo(indexFile)) {
                throw new IOException("Could not atomically publish cached SF2 index");
            }
        } finally {
            if (temporary.exists()) {
                temporary.delete();
            }
        }
    }

    private void ensureCacheDirectory() throws IOException {
        if (!cacheDirectory.isDirectory() && !cacheDirectory.mkdirs()) {
            throw new IOException("Could not create the private SF2 download cache");
        }
    }

    private static InputStream openHttps(String target) throws IOException {
        HttpsURLConnection connection = (HttpsURLConnection) new URL(target).openConnection();
        connection.setConnectTimeout(20000);
        connection.setReadTimeout(90000);
        connection.setInstanceFollowRedirects(false);
        connection.setUseCaches(false);
        connection.setRequestProperty("Accept-Encoding", "identity");
        try {
            int status = connection.getResponseCode();
            if (status != HttpsURLConnection.HTTP_OK) {
                throw new IOException("SF2 server returned HTTP " + status);
            }
            return new FilterInputStream(connection.getInputStream()) {
                @Override
                public void close() throws IOException {
                    try {
                        super.close();
                    } finally {
                        connection.disconnect();
                    }
                }
            };
        } catch (IOException failure) {
            connection.disconnect();
            throw failure;
        }
    }

    private static byte[] readLimited(InputStream input, int maximumBytes) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[BUFFER_SIZE];
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (output.size() > maximumBytes - count) {
                throw new IOException("SF2 index exceeds the permitted size");
            }
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    private static boolean isSafeRelativePath(String value) {
        if (value.isEmpty() || value.startsWith("/") || value.indexOf('\\') >= 0) {
            return false;
        }
        String[] parts = value.split("/", -1);
        for (String part : parts) {
            if (part.isEmpty() || ".".equals(part) || "..".equals(part)) {
                return false;
            }
            for (int index = 0; index < part.length(); index++) {
                char character = part.charAt(index);
                if (character < 0x20 || character == 0x7f) {
                    return false;
                }
            }
        }
        return true;
    }

    private static boolean isSha256(String value) {
        if (value.length() != 64) {
            return false;
        }
        for (int index = 0; index < value.length(); index++) {
            char character = value.charAt(index);
            if (!((character >= '0' && character <= '9') ||
                    (character >= 'a' && character <= 'f'))) {
                return false;
            }
        }
        return true;
    }

    private static String stripCarriageReturn(String value) {
        return value.endsWith("\r") ? value.substring(0, value.length() - 1) : value;
    }

    private static String toHex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) {
            result.append(String.format(Locale.ROOT, "%02x", value & 0xff));
        }
        return result.toString();
    }

    private static final class Asset {
        final String path;
        final String sha256;
        final long sizeBytes;

        Asset(String path, String sha256, long sizeBytes) {
            this.path = path;
            this.sha256 = sha256;
            this.sizeBytes = sizeBytes;
        }
    }
}
