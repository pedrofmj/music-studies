package org.musicrig.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;

import org.musicrig.core.MidiInput;
import org.musicrig.core.RigHost;
import org.musicrig.core.Sf2Profile;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class RigHostService extends Service {
    public static final String ACTION_START_PROFILE =
            "org.musicrig.android.action.START_PROFILE";
    public static final String ACTION_REFRESH_MIDI =
            "org.musicrig.android.action.REFRESH_MIDI";
    public static final String EXTRA_PROFILE_ID = "profile_id";
    public static final String EXTRA_SOUNDFONT_TREE = "soundfont_tree";
    public static final String CHANNEL_ID = "music-rig-audio";
    private static final int NOTIFICATION_ID = 1001;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private RigHost host;
    private MidiInput midiInput;
    private ProfileCatalog catalog;
    private Uri soundFontTree;

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        startForeground(NOTIFICATION_ID, notification("Music Rig starting"));
        try {
            catalog = ProfileCatalog.load(getAssets());
        } catch (Exception exception) {
            stopSelf();
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) {
            return START_STICKY;
        }
        if (intent.hasExtra(EXTRA_SOUNDFONT_TREE)) {
            soundFontTree = Uri.parse(intent.getStringExtra(EXTRA_SOUNDFONT_TREE));
        }
        if (ACTION_REFRESH_MIDI.equals(intent.getAction())) {
            if (midiInput != null) {
                midiInput.refresh();
            }
            return START_STICKY;
        }
        if (ACTION_START_PROFILE.equals(intent.getAction())) {
            String profileId = intent.getStringExtra(EXTRA_PROFILE_ID);
            if (profileId != null) {
                executor.execute(() -> selectProfile(profileId));
            }
        }
        return START_STICKY;
    }

    private void selectProfile(String profileId) {
        if (catalog == null || soundFontTree == null || !NativeFluidSynthEngine.isAvailable()) {
            return;
        }
        try {
            Sf2Profile profile = catalog.profile(profileId);
            if (host == null) {
                host = new RigHost(new NativeFluidSynthEngine(),
                        new SafSoundFontRepository(this, soundFontTree));
                startMidiInput();
            }
            host.selectProfile(profile);
            updateNotification("Music Rig: " + profile.label);
            preloadGenreCatalog();
        } catch (Exception exception) {
            updateNotification("Music Rig profile error");
        }
    }

    private void preloadGenreCatalog() {
        try {
            for (Sf2Profile genre : catalog.genreProfiles()) {
                host.preloadProfile(genre);
            }
            updateNotification("Music Rig: SF2 catalog ready");
        } catch (Exception exception) {
            updateNotification("Music Rig: SF2 prewarm incomplete");
        }
    }

    private void startMidiInput() {
        midiInput = new AndroidMidiInput(this, new MidiInput.Listener() {
            @Override
            public void onMidiMessage(byte[] message) {
                int padCode = org.musicrig.core.ArturiaPadModes.managementPadCode(message);
                if (padCode != 0) {
                    if (org.musicrig.core.ArturiaPadModes.isPress(message)) {
                        String profile = org.musicrig.core.ArturiaPadModes.profileForCode(padCode);
                        if (profile != null) {
                            selectProfile(profile);
                        }
                    }
                    return;
                }
                if (host != null) {
                    host.onMidiMessage(message);
                }
            }

            @Override
            public void onDeviceCountChanged(int count) {
                updateNotification("Music Rig MIDI devices: " + count);
            }
        });
        midiInput.start();
    }

    @Override
    public void onDestroy() {
        if (midiInput != null) {
            midiInput.close();
            midiInput = null;
        }
        executor.execute(() -> {
            if (host != null) {
                host.close();
                host = null;
            }
        });
        executor.shutdown();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, "Music Rig Audio", NotificationManager.IMPORTANCE_LOW
            );
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }
    }

    private Notification notification(String text) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);
        return builder.setContentTitle("Music Rig")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setOngoing(true)
                .build();
    }

    private void updateNotification(String text) {
        getSystemService(NotificationManager.class).notify(
                NOTIFICATION_ID, notification(text)
        );
    }
}
