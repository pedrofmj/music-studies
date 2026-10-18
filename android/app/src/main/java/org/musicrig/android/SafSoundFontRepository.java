package org.musicrig.android;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

import org.musicrig.core.SoundFontRepository;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.security.MessageDigest;

public final class SafSoundFontRepository implements SoundFontRepository {
    private final ContentResolver resolver;
    private final Uri treeUri;
    private final File cacheDirectory;

    public SafSoundFontRepository(Context context, Uri treeUri) {
        this.resolver = context.getContentResolver();
        this.treeUri = treeUri;
        this.cacheDirectory = new File(context.getCacheDir(), "selected-sf2");
    }

    @Override
    public String materialize(String relativePath) throws Exception {
        Uri document = findDocument(relativePath);
        if (document == null) {
            throw new IllegalArgumentException("SoundFont is not in the selected folder: " +
                    relativePath);
        }
        if (!cacheDirectory.isDirectory() && !cacheDirectory.mkdirs()) {
            throw new IllegalStateException("Could not create the private SoundFont cache");
        }
        File destination = new File(cacheDirectory, sha256(relativePath) + ".sf2");
        File temporary = File.createTempFile("sf2-", ".part", cacheDirectory);
        try {
            try (InputStream input = resolver.openInputStream(document);
                 FileOutputStream output = new FileOutputStream(temporary)) {
                if (input == null) {
                    throw new IllegalStateException("SAF provider returned no SoundFont stream");
                }
                byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    output.write(buffer, 0, count);
                }
                output.getFD().sync();
            }
            if (!temporary.renameTo(destination)) {
                throw new IllegalStateException("Could not atomically publish cached SoundFont");
            }
        } finally {
            if (temporary.exists()) {
                temporary.delete();
            }
        }
        return destination.getAbsolutePath();
    }

    private Uri findDocument(String relativePath) throws Exception {
        String documentId = DocumentsContract.getTreeDocumentId(treeUri);
        String[] components = relativePath.split("/");
        Uri found = null;
        for (int index = 0; index < components.length; index++) {
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri,
                    documentId);
            String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE};
            found = null;
            try (Cursor cursor = resolver.query(children, columns, null, null, null)) {
                if (cursor == null) {
                    return null;
                }
                while (cursor.moveToNext()) {
                    if (components[index].equals(cursor.getString(1))) {
                        boolean last = index == components.length - 1;
                        String mimeType = cursor.getString(2);
                        if (!last && !DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                            return null;
                        }
                        if (last && DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) {
                            return null;
                        }
                        documentId = cursor.getString(0);
                        found = DocumentsContract.buildDocumentUriUsingTree(treeUri, documentId);
                        break;
                    }
                }
            }
            if (found == null) {
                return null;
            }
        }
        return found;
    }

    private static String sha256(String value) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(
                value.getBytes(java.nio.charset.StandardCharsets.UTF_8));
        StringBuilder result = new StringBuilder(digest.length * 2);
        for (byte valueByte : digest) {
            result.append(String.format(java.util.Locale.ROOT, "%02x", valueByte & 0xff));
        }
        return result.toString();
    }
}
