package org.musicrig.android;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;

import org.musicrig.core.MidiInput;
import org.musicrig.core.MidiStreamParser;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

public final class AndroidMidiInput implements MidiInput {
    private static final ParcelUuid MIDI_BLE_SERVICE = new ParcelUuid(UUID.fromString(
            "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"));

    private final MidiManager midiManager;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final MidiInput.Listener listener;
    private final Map<Integer, MidiDevice> devices = new HashMap<>();
    private final Map<Integer, List<MidiOutputPort>> portsByDevice = new HashMap<>();
    private final Set<Integer> openingDevices = new HashSet<>();
    private final Set<String> openingBluetoothAddresses = new HashSet<>();
    private final Map<String, Integer> bluetoothDeviceIds = new HashMap<>();
    private boolean started;
    private BluetoothLeScanner bleScanner;

    private final MidiManager.DeviceCallback deviceCallback = new MidiManager.DeviceCallback() {
        @Override
        public void onDeviceAdded(MidiDeviceInfo info) {
            if (info.getType() == MidiDeviceInfo.TYPE_BLUETOOTH &&
                    isBluetoothAlreadyOpening(info)) {
                return;
            }
            openDevice(info);
        }

        @Override
        public void onDeviceRemoved(MidiDeviceInfo info) {
            closeDevice(info.getId());
        }
    };

    private final ScanCallback bleScanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            openBluetoothMidiDevice(result.getDevice());
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            for (ScanResult result : results) {
                openBluetoothMidiDevice(result.getDevice());
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            stopBleScan();
        }
    };
    private final Runnable stopScanTask = this::stopBleScan;

    public AndroidMidiInput(Context context, MidiInput.Listener listener) {
        this.midiManager = context.getSystemService(MidiManager.class);
        this.listener = listener;
    }

    @Override
    public synchronized void start() {
        if (midiManager == null || started) {
            return;
        }
        started = true;
        midiManager.registerDeviceCallback(deviceCallback, mainHandler);
        refresh();
    }

    @Override
    public synchronized void refresh() {
        if (!started) {
            return;
        }
        try {
            for (MidiDeviceInfo info : midiManager.getDevices()) {
                openDevice(info);
            }
        } catch (SecurityException exception) {
            // USB hotplug callbacks remain registered when Bluetooth access is denied.
        }
        startBleScan();
    }

    private synchronized void openDevice(MidiDeviceInfo info) {
        int id = info.getId();
        if (!started || (info.getType() != MidiDeviceInfo.TYPE_USB &&
                info.getType() != MidiDeviceInfo.TYPE_BLUETOOTH) ||
                info.getOutputPortCount() == 0 || devices.containsKey(id) ||
                !openingDevices.add(id)) {
            return;
        }
        try {
            midiManager.openDevice(info, device -> {
                synchronized (AndroidMidiInput.this) {
                    openingDevices.remove(id);
                    connectDevice(device, null);
                }
            }, mainHandler);
        } catch (SecurityException exception) {
            openingDevices.remove(id);
        }
    }

    private synchronized void openBluetoothMidiDevice(BluetoothDevice bluetoothDevice) {
        if (!started) {
            return;
        }
        final String address;
        try {
            address = bluetoothDevice.getAddress();
        } catch (SecurityException exception) {
            return;
        }
        if (bluetoothDeviceIds.containsKey(address) ||
                !openingBluetoothAddresses.add(address)) {
            return;
        }
        try {
            midiManager.openBluetoothDevice(bluetoothDevice, device -> {
                synchronized (AndroidMidiInput.this) {
                    openingBluetoothAddresses.remove(address);
                    connectDevice(device, address);
                }
            }, mainHandler);
        } catch (SecurityException | IllegalArgumentException exception) {
            openingBluetoothAddresses.remove(address);
        }
    }

    private synchronized void connectDevice(MidiDevice device, String bluetoothAddress) {
        if (!started || device == null) {
            if (device != null) {
                device.close();
            }
            return;
        }
        int id = device.getInfo().getId();
        if (devices.containsKey(id)) {
            device.close();
            if (bluetoothAddress != null) {
                bluetoothDeviceIds.put(bluetoothAddress, id);
            }
            return;
        }

        List<MidiOutputPort> openedPorts = new ArrayList<>();
        for (int index = 0; index < device.getInfo().getOutputPortCount(); index++) {
            MidiOutputPort port = device.openOutputPort(index);
            if (port == null) {
                continue;
            }
            MidiStreamParser parser = new MidiStreamParser(listener::onMidiMessage);
            try {
                port.connect(new MidiReceiver() {
                    @Override
                    public void onSend(byte[] data, int offset, int count, long timestamp)
                            throws IOException {
                        parser.accept(data, offset, count);
                    }
                });
                openedPorts.add(port);
            } catch (IOException exception) {
                port.close();
            }
        }
        if (openedPorts.isEmpty()) {
            device.close();
            return;
        }
        devices.put(id, device);
        portsByDevice.put(id, openedPorts);
        if (bluetoothAddress != null) {
            bluetoothDeviceIds.put(bluetoothAddress, id);
        }
        listener.onDeviceCountChanged(devices.size());
    }

    private synchronized boolean isBluetoothAlreadyOpening(MidiDeviceInfo info) {
        BluetoothDevice bluetoothDevice = (BluetoothDevice) info.getProperties().getParcelable(
                MidiDeviceInfo.PROPERTY_BLUETOOTH_DEVICE);
        if (bluetoothDevice == null) {
            return false;
        }
        try {
            String address = bluetoothDevice.getAddress();
            return openingBluetoothAddresses.contains(address) ||
                    bluetoothDeviceIds.containsKey(address);
        } catch (SecurityException exception) {
            return false;
        }
    }

    private void startBleScan() {
        try {
            stopBleScan();
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (adapter == null || !adapter.isEnabled()) {
                return;
            }
            bleScanner = adapter.getBluetoothLeScanner();
            if (bleScanner == null) {
                return;
            }
            bleScanner.startScan(Collections.singletonList(new ScanFilter.Builder()
                            .setServiceUuid(MIDI_BLE_SERVICE).build()),
                    new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                            .build(), bleScanCallback);
            mainHandler.removeCallbacks(stopScanTask);
            mainHandler.postDelayed(stopScanTask, 15000);
        } catch (SecurityException | IllegalStateException exception) {
            // The app still accepts USB MIDI when Bluetooth scan permission is unavailable.
        }
    }

    private synchronized void stopBleScan() {
        mainHandler.removeCallbacks(stopScanTask);
        if (bleScanner != null) {
            try {
                bleScanner.stopScan(bleScanCallback);
            } catch (SecurityException | IllegalStateException ignored) {
                // The scan is already unavailable after permission revocation.
            }
            bleScanner = null;
        }
    }

    private synchronized void closeDevice(int id) {
        List<MidiOutputPort> ports = portsByDevice.remove(id);
        if (ports != null) {
            for (MidiOutputPort port : ports) {
                port.close();
            }
        }
        MidiDevice device = devices.remove(id);
        if (device != null) {
            device.close();
        }
        Iterator<Map.Entry<String, Integer>> bluetoothEntries =
                bluetoothDeviceIds.entrySet().iterator();
        while (bluetoothEntries.hasNext()) {
            if (bluetoothEntries.next().getValue() == id) {
                bluetoothEntries.remove();
            }
        }
        listener.onDeviceCountChanged(devices.size());
    }

    @Override
    public synchronized void close() {
        if (!started) {
            return;
        }
        started = false;
        midiManager.unregisterDeviceCallback(deviceCallback);
        stopBleScan();
        for (int id : new ArrayList<>(devices.keySet())) {
            closeDevice(id);
        }
        openingDevices.clear();
        openingBluetoothAddresses.clear();
        bluetoothDeviceIds.clear();
    }
}
