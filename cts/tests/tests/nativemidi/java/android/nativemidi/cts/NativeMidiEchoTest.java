/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.nativemidi.cts;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Bundle;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.midi.MidiEchoTestService;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.Random;

/*
 * Test Class
 */
@RunWith(AndroidJUnit4.class)
public class NativeMidiEchoTest {
    private static final String TAG = "NativeMidiEchoTest";

    private static final long NANOS_PER_MSEC = 1000L * 1000L;

    // This number seems excessively large and it is not clear if there is a linear
    // relationship between the number of messages sent and the time required to send them
    private static final int TIMEOUT_PER_MESSAGE_MS = 10;

    // This timeout value is very generous.
    private static final int TIMEOUT_OPEN_MSEC = 2000; // arbitrary

    private Context mContext = InstrumentationRegistry.getContext();
    private MidiManager mMidiManager;

    private MidiDevice mEchoDevice;

    private Random mRandom = new Random(1972941337);

    // (Native code) attributes associated with a test/EchoServer instance.
    private long mTestContext;

    static {
        System.loadLibrary("nativemidi_jni");
    }

    /*
     * Helpers
     */
    private boolean hasMidiSupport() {
        PackageManager pm = mContext.getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_MIDI);
    }

    public static boolean hasLibAMidi() {
        try {
            System.loadLibrary("amidi");
        } catch (UnsatisfiedLinkError ex) {
            Log.e(TAG, "libamidi.so not found.");
            return false;
        }
        return true;
    }

    private byte[] generateRandomMessage(int len) {
        byte[] buffer = new byte[len];
        for(int index = 0; index < len; index++) {
            buffer[index] = (byte)(mRandom.nextInt() & 0xFF);
        }
        return buffer;
    }

    private void generateRandomBufers(byte[][] buffers, long timestamps[], int numMessages) {
        int messageLen;
        int maxMessageLen = 128;
        for(int buffIndex = 0; buffIndex < numMessages; buffIndex++) {
            messageLen = (int)(mRandom.nextFloat() * (maxMessageLen-1)) + 1;
            buffers[buffIndex] = generateRandomMessage(messageLen);
            timestamps[buffIndex] = mRandom.nextLong();
        }
    }

    private void compareMessages(byte[] buffer, long timestamp, NativeMidiMessage nativeMsg) {
        Assert.assertEquals("byte count of message", buffer.length, nativeMsg.len);
        Assert.assertEquals("timestamp in message", timestamp, nativeMsg.timestamp);

        for (int index = 0; index < buffer.length; index++) {
            Assert.assertEquals("message byte[" + index + "]", buffer[index] & 0x0FF,
                    nativeMsg.buffer[index] & 0x0FF);
        }
    }

    /*
     * Echo Server
     */
    // Listens for an asynchronous device open and notifies waiting foreground
    // test.
    class MyTestOpenCallback implements MidiManager.OnDeviceOpenedListener {
        private volatile MidiDevice mDevice;

        @Override
        public synchronized void onDeviceOpened(MidiDevice device) {
            mDevice = device;
            notifyAll();
        }

        public synchronized MidiDevice waitForOpen(int msec)
              throws InterruptedException {
            long deadline = System.currentTimeMillis() + msec;
            long timeRemaining = msec;
            while (mDevice == null && timeRemaining > 0) {
                wait(timeRemaining);
                timeRemaining = deadline - System.currentTimeMillis();
            }
            return mDevice;
        }
    }

     protected void setUpEchoServer() throws Exception {
        MidiDeviceInfo echoInfo = MidiEchoTestService.findEchoDevice(mContext);

        // Open device.
        MyTestOpenCallback callback = new MyTestOpenCallback();
        mMidiManager.openDevice(echoInfo, callback, null);
        mEchoDevice = callback.waitForOpen(TIMEOUT_OPEN_MSEC);
        Assert.assertNotNull(
                "could not open " + MidiEchoTestService.getEchoServerName(), mEchoDevice);

        // Query echo service directly to see if it is getting status updates.
        MidiEchoTestService echoService = MidiEchoTestService.getInstance();

        mTestContext = allocTestContext();
        Assert.assertTrue("couldn't allocate test context.", mTestContext != 0);

        // Open Device
        int result = openNativeMidiDevice(mTestContext, mEchoDevice);
        Assert.assertEquals("Bad open native MIDI device", 0, result);

        // Open Input
        result = startWritingMidi(mTestContext, 0/*mPortNumber*/);
        Assert.assertEquals("Bad start writing (native) MIDI", 0, result);

        // Open Output
        result = startReadingMidi(mTestContext, 0/*mPortNumber*/);
        Assert.assertEquals("Bad start Reading (native) MIDI", 0, result);
    }

    protected void tearDownEchoServer() throws IOException {
        // Query echo service directly to see if it is getting status updates.
        MidiEchoTestService echoService = MidiEchoTestService.getInstance();

        int result;

        // Stop inputs
        result = stopReadingMidi(mTestContext);
        Assert.assertEquals("Bad stop reading (native) MIDI", 0, result);

        // Stop outputs
        result = stopWritingMidi(mTestContext);
        Assert.assertEquals("Bad stop writing (native) MIDI", 0, result);

        // Close Device
        result = closeNativeMidiDevice(mTestContext);
        Assert.assertEquals("Bad close native MIDI device", 0, result);

        freeTestContext(mTestContext);
        mTestContext = 0;

        mEchoDevice.close();
    }

    // Search through the available devices for the ECHO loop-back device.
//    protected MidiDeviceInfo findEchoDevice() {
//        MidiDeviceInfo[] infos = mMidiManager.getDevices();
//        MidiDeviceInfo echoInfo = null;
//        for (MidiDeviceInfo info : infos) {
//            Bundle properties = info.getProperties();
//            String manufacturer = (String) properties.get(
//                    MidiDeviceInfo.PROPERTY_MANUFACTURER);
//
//            if (TEST_MANUFACTURER.equals(manufacturer)) {
//                String product = (String) properties.get(
//                        MidiDeviceInfo.PROPERTY_PRODUCT);
//                if (MidiEchoTestService.getEchoServerName().equals(product)) {
//                    echoInfo = info;
//                    break;
//                }
//            }
//        }
//        Assert.assertNotNull("could not find " + MidiEchoTestService.getEchoServerName(), echoInfo);
//        return echoInfo;
//    }
//
    @Before
    public void setUp() throws Exception {
        if (!hasMidiSupport()) {
            return; // Not supported so don't test it.
        }

        mMidiManager = (MidiManager)mContext.getSystemService(Context.MIDI_SERVICE);
        Assert.assertNotNull("Could not get the MidiManger.", mMidiManager);

        setUpEchoServer();
    }

    @After
    public void tearDown() throws Exception {
        if (!hasMidiSupport()) {
            return; // Not supported so don't test it.
        }
        tearDownEchoServer();

        mMidiManager = null;
    }

    @Test
    public void test_A_MidiManager() throws Exception {
        if (!hasMidiSupport()) {
            return; // Nothing to test
        }

        Assert.assertNotNull("MidiManager not supported.", mMidiManager);

        // There should be at least one device for the Echo server.
        MidiDeviceInfo[] infos = mMidiManager.getDevices();
        Assert.assertNotNull("device list was null", infos);
        Assert.assertTrue("device list was empty", infos.length >= 1);
    }


    @Test
    public void test_AA_LibAMidiExists() throws Exception {
        if (!hasMidiSupport()) {
            return;
        }
        Assert.assertTrue("libamidi.so not found.", hasLibAMidi());
    }

    @Test
    public void test_B_SendData() throws Exception {
        if (!hasMidiSupport()) {
            return; // Nothing to test
        }

        Assert.assertEquals("Didn't start with 0 sends", 0, getNumSends(mTestContext));
        Assert.assertEquals("Didn't start with 0 bytes sent", 0, getNumBytesSent(mTestContext));

        final byte[] buffer = {
                (byte) 0x93, 0x47, 0x52
        };
        long timestamp = 0x0123765489ABFEDCL;
        writeMidi(mTestContext, buffer, 0, buffer.length);

        Assert.assertEquals("Didn't get right number of bytes sent",
                buffer.length, getNumBytesSent(mTestContext));
    }

    @Test
    public void test_C_EchoSmallMessage() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        final byte[] buffer = {
                (byte) 0x93, 0x47, 0x52
        };
        long timestamp = 0x0123765489ABFEDCL;

        writeMidiWithTimestamp(mTestContext, buffer, 0, 0, timestamp); // should be a NOOP
        writeMidiWithTimestamp(mTestContext, buffer, 0, buffer.length, timestamp);
        writeMidiWithTimestamp(mTestContext, buffer, 0, 0, timestamp); // should be a NOOP

        // Wait for message to pass quickly through echo service.
        final int numMessages = 1;
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);
        Assert.assertEquals("number of messages.",
                numMessages, getNumReceivedMessages(mTestContext));

        NativeMidiMessage message = getReceivedMessageAt(mTestContext, 0);
        compareMessages(buffer, timestamp, message);
    }

    @Test
    public void test_D_EchoNMessages() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        int numMessages = 100;
        byte[][] buffers = new byte[numMessages][];
        long timestamps[] = new long[numMessages];
        generateRandomBufers(buffers, timestamps, numMessages);

        for(int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            writeMidiWithTimestamp(mTestContext, buffers[msgIndex], 0, buffers[msgIndex].length,
                    timestamps[msgIndex]);
        }

        // Wait for message to pass quickly through echo service.
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);

        // correct number of messages
        Assert.assertEquals("number of messages.",
                numMessages, getNumReceivedMessages(mTestContext));

        // correct data & order?
        for(int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            NativeMidiMessage message = getReceivedMessageAt(mTestContext, msgIndex);
            compareMessages(buffers[msgIndex], timestamps[msgIndex], message);
        }
    }

    @Test
    public void test_E_FlushMessages() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        int numMessages = 7;
        byte[][] buffers = new byte[numMessages][];
        long timestamps[] = new long[numMessages];
        generateRandomBufers(buffers, timestamps, numMessages);

        for(int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            writeMidiWithTimestamp(mTestContext, buffers[msgIndex], 0, buffers[msgIndex].length,
              timestamps[msgIndex]);
        }

        // Wait for message to pass through echo service.
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);

        int result = flushSentMessages(mTestContext);
        Assert.assertEquals("flush messages failed", 0, result);

        // correct number of messages
        Assert.assertEquals("number of messages.",
                numMessages, getNumReceivedMessages(mTestContext));

        // correct data & order?
        for(int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            NativeMidiMessage message = getReceivedMessageAt(mTestContext, msgIndex);
            compareMessages(buffers[msgIndex], timestamps[msgIndex], message);
        }
    }

    @Test
    public void test_F_HugeMessage() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        // Arbitrarily large message.
        int hugeMessageLen = 1024 * 10;
        byte[] buffer = generateRandomMessage(hugeMessageLen);
        int result = writeMidi(mTestContext, buffer, 0, buffer.length);
        Assert.assertEquals("Huge write failed.", hugeMessageLen, result);

        int kindaHugeMessageLen = 1024 * 2;
        buffer = generateRandomMessage(kindaHugeMessageLen);
        result = writeMidi(mTestContext, buffer, 0, buffer.length);
        Assert.assertEquals("Kinda big write failed.", kindaHugeMessageLen, result);
    }

    /**
     * Check a large timeout for the echoed messages to come through. If they exceed this
     * or don't come through at all, something is wrong.
     */
    @Test
    public void test_G_NativeEchoTime() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        final int numMessages = 10;
        final long maxLatencyNanos = 15 * NANOS_PER_MSEC; // generally < 3 msec on N6
        byte[] buffer = { (byte) 0x93, 0, 64 };

        // Send multiple messages in a burst.
        for (int index = 0; index < numMessages; index++) {
            buffer[1] = (byte) (60 + index);
            writeMidiWithTimestamp(mTestContext, buffer, 0, buffer.length, System.nanoTime());
        }

        // Wait for messages to pass quickly through echo service.
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);
        Assert.assertEquals("number of messages.",
                numMessages, getNumReceivedMessages(mTestContext));

        for (int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            NativeMidiMessage message = getReceivedMessageAt(mTestContext, msgIndex);
            Assert.assertEquals("message index", (byte) (60 + msgIndex), message.buffer[1]);
            long elapsedNanos = message.timeReceived - message.timestamp;
            // If this test fails then there may be a problem with the thread scheduler
            // or there may be kernel activity that is blocking execution at the user level.
            Assert.assertTrue("MIDI round trip latency index:" + msgIndex
                    + " too large, " + elapsedNanos
                    + " nanoseconds " +
                    "timestamp:" + message.timestamp + " received:" + message.timeReceived,
                    (elapsedNanos < maxLatencyNanos));
        }
    }

    @Test
    public void test_H_EchoNMessages_PureNative() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        int numMessages = 2;
        byte[][] buffers = new byte[numMessages][];
        long timestamps[] = new long[numMessages];
        generateRandomBufers(buffers, timestamps, numMessages);

        for(int msgIndex = 0; msgIndex < numMessages; msgIndex++) {
            writeMidiWithTimestamp(mTestContext, buffers[msgIndex], 0, buffers[msgIndex].length,
                    timestamps[msgIndex]);
        }

        // Wait for message to pass quickly through echo service.
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);

        int result = matchNativeMessages(mTestContext);
        Assert.assertEquals("Native Compare Test Failed", result, 0);
    }

    /**
     * Check a large timeout for the echoed messages to come through. If they exceed this
     * or don't come through at all, something is wrong.
     */
    @Test
    public void test_I_NativeEchoTime_PureNative() throws Exception {
        if (!hasMidiSupport()) {
            return; // nothing to test
        }

        final int numMessages = 10;
        final long maxLatencyNanos = 15 * NANOS_PER_MSEC; // generally < 3 msec on N6
        byte[] buffer = { (byte) 0x93, 0, 64 };

        // Send multiple messages in a burst.
        for (int index = 0; index < numMessages; index++) {
            buffer[1] = (byte) (60 + index);
            writeMidiWithTimestamp(mTestContext, buffer, 0, buffer.length, System.nanoTime());
        }

        // Wait for messages to pass through echo service.
        final int timeoutMs = TIMEOUT_PER_MESSAGE_MS * numMessages;
        Thread.sleep(timeoutMs);
        Assert.assertEquals("number of messages.",
                numMessages, getNumReceivedMessages(mTestContext));

        int result = checkNativeLatency(mTestContext, maxLatencyNanos);
        Assert.assertEquals("failed pure native latency test.", 0, result);
    }

    // Native Routines
    public static native void initN();

    public static native long allocTestContext();
    public static native void freeTestContext(long context);

    public native int openNativeMidiDevice(long ctx, MidiDevice device);
    public native int closeNativeMidiDevice(long ctx);

    public native int startReadingMidi(long ctx, int portNumber);
    public native int stopReadingMidi(long ctx);

    public native int startWritingMidi(long ctx, int portNumber);
    public native int stopWritingMidi(long ctx);

    public native int writeMidi(long ctx, byte[] data, int offset, int length);
    public native int writeMidiWithTimestamp(long ctx, byte[] data, int offset, int length,
            long timestamp);
    public native int flushSentMessages(long ctx);

    // Status - Counters
    public native int getNumSends(long ctx);
    public native int getNumBytesSent(long ctx);
    public native int getNumReceives(long ctx);
    public native int getNumBytesReceived(long ctx);

    // Status - Received Messages
    public native int getNumReceivedMessages(long ctx);
    public native NativeMidiMessage getReceivedMessageAt(long ctx, int index);

    // Pure Native Checks
    public native int matchNativeMessages(long ctx);
    public native int checkNativeLatency(long ctx, long maxLatencyNanos);
}
