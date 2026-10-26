package org.musicrig.android;

import android.content.Context;

import org.musicrig.core.SoundFontRepository;
import org.musicrig.core.VerifiedSf2Pack;

import java.io.File;

public final class CloudSoundFontRepository implements SoundFontRepository {
    public static final String BASE_URL =
            "https://echora.egypteam.com/downloads/music-rig/sf2/v1/android/";

    private final VerifiedSf2Pack pack;

    public CloudSoundFontRepository(Context context) {
        File cacheDirectory = new File(context.getFilesDir(), "sf2/android-v1");
        pack = new VerifiedSf2Pack(BASE_URL, cacheDirectory);
    }

    @Override
    public String materialize(String relativePath) throws Exception {
        return pack.materialize(relativePath);
    }

    public int prefetchAll(VerifiedSf2Pack.ProgressListener listener) throws Exception {
        return pack.prefetchAll(listener);
    }
}
