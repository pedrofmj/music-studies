package org.musicrig.core;

public final class MidiStreamParser {
    public interface Listener {
        void onMessage(byte[] message);
    }

    private final Listener listener;
    private int runningStatus;
    private int currentStatus;
    private int expectedData;
    private int dataCount;
    private final byte[] data = new byte[2];

    public MidiStreamParser(Listener listener) {
        this.listener = listener;
    }

    public void accept(byte[] bytes, int offset, int length) {
        for (int index = offset; index < offset + length; index++) {
            int value = bytes[index] & 0xff;
            if (value >= 0xF8) {
                continue;
            }
            if ((value & 0x80) != 0) {
                beginStatus(value);
            } else if (expectedData > 0) {
                data[dataCount++] = (byte) value;
                if (dataCount == expectedData) {
                    byte[] message = new byte[expectedData + 1];
                    message[0] = (byte) currentStatus;
                    System.arraycopy(data, 0, message, 1, expectedData);
                    listener.onMessage(message);
                    if (currentStatus >= 0xF0) {
                        expectedData = 0;
                    } else {
                        currentStatus = runningStatus;
                        dataCount = 0;
                    }
                }
            }
        }
    }

    private void beginStatus(int status) {
        dataCount = 0;
        currentStatus = status;
        if (status >= 0xF0) {
            runningStatus = 0;
            expectedData = status == 0xF1 || status == 0xF3 ? 1 :
                    status == 0xF2 ? 2 : 0;
        } else {
            runningStatus = status;
            int kind = status & 0xF0;
            expectedData = kind == 0xC0 || kind == 0xD0 ? 1 : 2;
        }
        if (expectedData == 0 && status != 0xF0 && status != 0xF7) {
            listener.onMessage(new byte[]{(byte) status});
        }
    }
}
