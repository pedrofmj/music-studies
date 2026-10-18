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

import org.musicrig.core.RigHost;
import org.musicrig.core.MidiInput;
import org.musicrig.core.ArturiaPadModes;
import org.musicrig.core.Sf2Profile;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final int REQUEST_SF2_TREE = 1;
    private static final int REQUEST_BLUETOOTH_CONNECT = 2;

    private final ExecutorService controlExecutor = Executors.newSingleThreadExecutor();
    private TextView folderStatus;
    private TextView profileStatus;
    private TextView midiStatus;
    private Spinner genreSpinner;
    private ProfileCatalog catalog;
    private Uri soundFontTree;
    private volatile RigHost host;
    private MidiInput midiInput;

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
        if (midiInput != null) {
            midiInput.close();
            midiInput = null;
        }
        controlExecutor.execute(() -> {
            if (host != null) {
                host.close();
                host = null;
            }
        });
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
        selectProfile(option.id, false);
    }

    private void selectProfile(String profileId, boolean fromPad) {
        final Sf2Profile profile;
        try {
            profile = catalog.profile(profileId);
        } catch (Exception exception) {
            toast("Could not prepare profile: " + exception.getMessage());
            return;
        }
        controlExecutor.execute(() -> {
            try {
                if (host == null) {
                    NativeFluidSynthEngine engine = new NativeFluidSynthEngine();
                    host = new RigHost(engine,
                            new SafSoundFontRepository(this, soundFontTree));
                    runOnUiThread(this::startMidiInput);
                }
                host.selectProfile(profile);
                runOnUiThread(() -> {
                    for (int index = 0; index < genreSpinner.getCount(); index++) {
                        ProfileCatalog.ProfileOption option =
                                (ProfileCatalog.ProfileOption) genreSpinner.getItemAtPosition(index);
                        if (profileId.equals(option.id)) {
                            genreSpinner.setSelection(index);
                            break;
                        }
                    }
                    profileStatus.setText(profile.label + "\n" + profile.mappingDescription);
                    if (fromPad) {
                        toast("Pad-selected profile: " + profile.label);
                    } else {
                        toast("Profile active: " + profile.label);
                    }
                });
            } catch (Exception exception) {
                runOnUiThread(() -> toast("Could not start profile: " +
                        exception.getMessage()));
            }
        });
    }

    private void startMidiInput() {
        midiInput = new AndroidMidiInput(this, new MidiInput.Listener() {
            @Override
            public void onMidiMessage(byte[] message) {
                int padCode = ArturiaPadModes.managementPadCode(message);
                if (padCode != 0) {
                    if (ArturiaPadModes.isPress(message)) {
                        String profile = ArturiaPadModes.profileForCode(padCode);
                        if (profile == null) {
                            runOnUiThread(() -> toast("This pad mode is not implemented on Android yet"));
                        } else {
                            selectProfile(profile, true);
                        }
                    }
                    return;
                }
                RigHost current = host;
                if (current != null) {
                    current.onMidiMessage(message);
                }
            }

            @Override
            public void onDeviceCountChanged(int count) {
                runOnUiThread(() -> midiStatus.setText(count == 0
                        ? "Waiting for USB or paired BLE MIDI input"
                        : "MIDI devices connected: " + count));
            }
        });
        midiInput.start();
        requestBluetoothPermissions();
    }

    private void rescanMidiDevices() {
        if (midiInput == null) {
            toast("Start a profile before scanning for MIDI devices");
            return;
        }
        midiInput.refresh();
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
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_BLUETOOTH_CONNECT && midiInput != null) {
            midiInput.refresh();
        }
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
        if (midiInput != null) {
            midiInput.close();
        }
        controlExecutor.execute(() -> {
            if (host != null) {
                host.close();
                host = null;
            }
        });
        controlExecutor.shutdown();
        super.onDestroy();
    }
}
