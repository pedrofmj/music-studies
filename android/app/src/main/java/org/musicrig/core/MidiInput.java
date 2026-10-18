package org.musicrig.core;

public interface MidiInput extends AutoCloseable {
    interface Listener {
        void onMidiMessage(byte[] message);
        void onDeviceCountChanged(int count);
    }

    void start();
    void refresh();
    @Override
    void close();
}
