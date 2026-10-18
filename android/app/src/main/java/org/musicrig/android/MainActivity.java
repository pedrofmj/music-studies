package org.musicrig.android;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

public final class MainActivity extends Activity {
    private static final int REQUEST_SF2_TREE = 1;
    private static final int REQUEST_BLUETOOTH_CONNECT = 2;

    private TextView folderStatus;
    private TextView profileStatus;
    private TextView midiStatus;
    private Spinner genreSpinner;
    private ProfileCatalog catalog;
    private Uri soundFontTree;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        try {
            catalog = ProfileCatalog.load(getAssets());
        } catch (Exception exception) {
            showFatal("Could not load the shared Arturia routing catalog: " +
                    exception.getMessage());
            return;
        }
        buildUi();
        String savedTree = getPreferences(MODE_PRIVATE).getString("soundfont_tree", null);
        if (savedTree != null) {
            soundFontTree = Uri.parse(savedTree);
            folderStatus.setText("Selected SoundFont folder is remembered");
        }
    }

    private void buildUi() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        content.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setText("Music Rig Android");
        title.setTextSize(24);
        content.addView(title, matchWrap());

        folderStatus = new TextView(this);
        folderStatus.setText("Choose the folder containing the SoundFont catalog paths.");
        content.addView(folderStatus, matchWrap());
        Button chooseFolder = new Button(this);
        chooseFolder.setText("Choose SoundFont folder");
        chooseFolder.setOnClickListener(view -> chooseSoundFontFolder());
        content.addView(chooseFolder, matchWrap());

        genreSpinner = new Spinner(this);
        genreSpinner.setAdapter(new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_dropdown_item, catalog.profileOptions()));
        content.addView(genreSpinner, matchWrap());

        Button start = new Button(this);
        start.setText("Start / switch profile");
        start.setOnClickListener(view -> startSelectedProfile());
        content.addView(start, matchWrap());

        profileStatus = new TextView(this);
        profileStatus.setText("Select a startup Organ or genre profile.");
        content.addView(profileStatus, matchWrap());

        midiStatus = new TextView(this);
        midiStatus.setText("MIDI input not connected");
        content.addView(midiStatus, matchWrap());
        Button rescanMidi = new Button(this);
        rescanMidi.setText("Rescan MIDI devices");
        rescanMidi.setOnClickListener(view -> rescanMidiDevices());
        content.addView(rescanMidi, matchWrap());
        setContentView(content);
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private void chooseSoundFontFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_SF2_TREE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_SF2_TREE || resultCode != RESULT_OK || data == null ||
                data.getData() == null) {
            return;
        }
        soundFontTree = data.getData();
        int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION |
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        getContentResolver().takePersistableUriPermission(soundFontTree, flags);
        getPreferences(MODE_PRIVATE).edit().putString("soundfont_tree",
                soundFontTree.toString()).apply();
        stopService(new Intent(this, RigHostService.class));
        folderStatus.setText("SoundFont folder selected");
    }

    private void startSelectedProfile() {
        if (soundFontTree == null) {
            toast("Choose a SoundFont folder first");
            return;
        }
        if (!NativeFluidSynthEngine.isAvailable()) {
            toast("FluidSynth Android native bundle is missing. See android/README.md");
            return;
        }
        ProfileCatalog.ProfileOption option =
                (ProfileCatalog.ProfileOption) genreSpinner.getSelectedItem();
        startRigService(option.id);
    }

    private void startRigService(String profileId) {
        Intent service = new Intent(this, RigHostService.class)
                .setAction(RigHostService.ACTION_START_PROFILE)
                .putExtra(RigHostService.EXTRA_PROFILE_ID, profileId)
                .putExtra(RigHostService.EXTRA_SOUNDFONT_TREE, soundFontTree.toString());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(service);
        } else {
            startService(service);
        }
        requestBluetoothPermissions();
        profileStatus.setText("Starting profile: " + profileId);
    }

    private void rescanMidiDevices() {
        if (soundFontTree == null) {
            toast("Choose a SoundFont folder first");
            return;
        }
        Intent service = new Intent(this, RigHostService.class)
                .setAction(RigHostService.ACTION_REFRESH_MIDI);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(service);
        } else {
            startService(service);
        }
        requestBluetoothPermissions();
    }

    private void requestBluetoothPermissions() {
        if (Build.VERSION.SDK_INT >= 31) {
            boolean scanDenied = checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)
                    != PackageManager.PERMISSION_GRANTED;
            boolean connectDenied = checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                    != PackageManager.PERMISSION_GRANTED;
            if (scanDenied || connectDenied) {
                requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,
                        Manifest.permission.BLUETOOTH_CONNECT}, REQUEST_BLUETOOTH_CONNECT);
            }
        } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                    REQUEST_BLUETOOTH_CONNECT);
        }
        if (Build.VERSION.SDK_INT >= 33 &&
                checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
                        PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS},
                    REQUEST_BLUETOOTH_CONNECT);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    private void showFatal(String message) {
        TextView text = new TextView(this);
        text.setText(message);
        setContentView(text);
    }

    private void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
