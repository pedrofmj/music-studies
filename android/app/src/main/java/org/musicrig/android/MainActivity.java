package org.musicrig.android;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
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
    private static final int REQUEST_BLUETOOTH_CONNECT = 2;

    private TextView packStatus;
    private TextView profileStatus;
    private TextView midiStatus;
    private Spinner genreSpinner;
    private ProfileCatalog catalog;

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

        packStatus = new TextView(this);
        packStatus.setText("Selected-profile SF2 files download securely as needed. " +
                "Full Android pack: about 495 MB.");
        content.addView(packStatus, matchWrap());
        Button prefetchPack = new Button(this);
        prefetchPack.setText("Download Full SF2 Pack (495 MB)");
        prefetchPack.setOnClickListener(view -> confirmPackPrefetch());
        content.addView(prefetchPack, matchWrap());

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

    private void startSelectedProfile() {
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
                .putExtra(RigHostService.EXTRA_PROFILE_ID, profileId);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(service);
        } else {
            startService(service);
        }
        requestBluetoothPermissions();
        profileStatus.setText("Starting profile: " + profileId);
    }

    private void rescanMidiDevices() {
        Intent service = new Intent(this, RigHostService.class)
                .setAction(RigHostService.ACTION_REFRESH_MIDI);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(service);
        } else {
            startService(service);
        }
        requestBluetoothPermissions();
    }

    private void confirmPackPrefetch() {
        new AlertDialog.Builder(this)
                .setTitle("Download the full SF2 pack?")
                .setMessage("This downloads about 495 MB to this device. Mobile data " +
                        "charges may apply.")
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Download", (dialog, which) -> startPackPrefetch())
                .show();
    }

    private void startPackPrefetch() {
        Intent service = new Intent(this, RigHostService.class)
                .setAction(RigHostService.ACTION_PREFETCH_SF2_PACK);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(service);
        } else {
            startService(service);
        }
        packStatus.setText("Full SF2 pack download started. Progress appears in the " +
                "Music Rig notification.");
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
