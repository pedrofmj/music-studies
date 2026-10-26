package org.musicrig.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
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
    public static final String ACTION_PREFETCH_SF2_PACK =
            "org.musicrig.android.action.PREFETCH_SF2_PACK";
    public static final String EXTRA_PROFILE_ID = "profile_id";
    public static final String CHANNEL_ID = "music-rig-audio";
    private static final int NOTIFICATION_ID = 1001;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private RigHost host;
    private MidiInput midiInput;
    private ProfileCatalog catalog;
    private CloudSoundFontRepository soundFonts;
    private volatile boolean profileRequested;
    private volatile boolean prefetchRunning;

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        startForeground(NOTIFICATION_ID, notification("Music Rig ready", true));
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
        if (ACTION_REFRESH_MIDI.equals(intent.getAction())) {
            if (midiInput != null) {
                midiInput.refresh();
            } else if (host == null && !profileRequested && !prefetchRunning) {
                stopSelf(startId);
            }
            return START_STICKY;
        }
        if (ACTION_PREFETCH_SF2_PACK.equals(intent.getAction())) {
            if (!prefetchRunning) {
                prefetchRunning = true;
                executor.execute(this::prefetchSoundFontPack);
            }
            return START_NOT_STICKY;
        }
        if (ACTION_START_PROFILE.equals(intent.getAction())) {
            String profileId = intent.getStringExtra(EXTRA_PROFILE_ID);
            if (profileId != null) {
                profileRequested = true;
                executor.execute(() -> selectProfile(profileId));
            }
        }
        return START_STICKY;
    }

    private void selectProfile(String profileId) {
        if (catalog == null || !NativeFluidSynthEngine.isAvailable()) {
            return;
        }
        updateNotification("Downloading selected SF2 files");
        try {
            Sf2Profile profile = catalog.profile(profileId);
            if (host == null) {
                host = new RigHost(new NativeFluidSynthEngine(),
                        soundFontRepository());
                startMidiInput();
            }
            host.selectProfile(profile);
            updateNotification("Music Rig: " + profile.label);
        } catch (Exception exception) {
            updateNotification("Music Rig profile error");
        }
    }

    private void prefetchSoundFontPack() {
        updateNotification("Preparing full SF2 pack download");
        try {
            int count = soundFontRepository().prefetchAll((completed, total) ->
                    updateNotification("Verified SF2 pack assets: " + completed + "/" + total));
            String result = "Full SF2 pack ready (" + count + " assets verified)";
            if (profileRequested) {
                updateNotification(result);
            } else {
                finishPrefetch(result);
            }
        } catch (Exception exception) {
            if (profileRequested) {
                updateNotification("SF2 pack download failed");
            } else {
                finishPrefetch("SF2 pack download failed");
            }
        } finally {
            prefetchRunning = false;
        }
    }

    private CloudSoundFontRepository soundFontRepository() {
        if (soundFonts == null) {
            soundFonts = new CloudSoundFontRepository(this);
        }
        return soundFonts;
    }

    private void finishPrefetch(String text) {
        getSystemService(NotificationManager.class).notify(
                NOTIFICATION_ID, notification(text, false));
        stopForeground(false);
        stopSelf();
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
        return notification(text, true);
    }

    private Notification notification(String text, boolean ongoing) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);
        return builder.setContentTitle("Music Rig")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setOngoing(ongoing)
                .build();
    }

    private void updateNotification(String text) {
        getSystemService(NotificationManager.class).notify(
                NOTIFICATION_ID, notification(text)
        );
    }
}
