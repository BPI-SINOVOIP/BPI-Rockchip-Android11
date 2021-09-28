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

package com.android.cts.verifier.audio;

import java.io.IOException;
import java.util.Timer;
import java.util.TimerTask;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;  // needed to access resource in CTSVerifier project namespace.

import com.android.midi.MidiEchoTestService;

/*
 * A note about the USB MIDI device.
 * Any USB MIDI peripheral with standard female DIN jacks can be used. A standard MIDI cable
 * plugged into both input and output is required for the USB Loopback Test. A Bluetooth MIDI
 * device like the Yamaha MD-BT01 plugged into both input and output is required for the
 * Bluetooth Loopback test.
 */

/*
 *  A note about the "virtual MIDI" device...
 * See file MidiEchoTestService for implementation of the echo server itself.
 * This service is started by the main manifest file (AndroidManifest.xml).
 */

/*
 * A note about Bluetooth MIDI devices...
 * Any Bluetooth MIDI device needs to be paired with the DUT with the "MIDI+BTLE" application
 * available in the Play Store:
 * (https://play.google.com/store/apps/details?id=com.mobileer.example.midibtlepairing).
 */

/**
 * CTS Verifier Activity for MIDI test
 */
public class MidiActivity extends PassFailButtons.Activity implements View.OnClickListener {

    private static final String TAG = "MidiActivity";
    private static final boolean DEBUG = false;

    private MidiManager mMidiManager;

    // Flags
    private boolean mHasMIDI;

    // Test "Modules"
    private MidiTestModule      mUSBTestModule;
    private MidiTestModule      mVirtTestModule;
    private BTMidiTestModule    mBTTestModule;

    // Test State
    private final Object    mTestLock = new Object();
    private boolean     mTestRunning;

    // Timeout handling
    private static final int TEST_TIMEOUT_MS = 1000;
    private final Timer mTimeoutTimer = new Timer();

    // Widgets
    private Button      mUSBTestBtn;
    private Button      mVirtTestBtn;
    private Button      mBTTestBtn;

    private TextView    mUSBIInputDeviceLbl;
    private TextView    mUSBOutputDeviceLbl;
    private TextView    mUSBTestStatusTxt;

    private TextView    mVirtInputDeviceLbl;
    private TextView    mVirtOutputDeviceLbl;
    private TextView    mVirtTestStatusTxt;

    private TextView    mBTInputDeviceLbl;
    private TextView    mBTOutputDeviceLbl;
    private TextView    mBTTestStatusTxt;

    private Intent mMidiServiceIntent;
    private MidiServiceConnection mMidiServiceConnection;

    public MidiActivity() {
        super();
    }

    private boolean hasMIDI() {
        // CDD Section C-1-4: android.software.midi
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI);
    }

    private void showConnectedMIDIPeripheral() {
        // USB
        mUSBIInputDeviceLbl.setText(mUSBTestModule.getInputName());
        mUSBOutputDeviceLbl.setText(mUSBTestModule.getOutputName());
        mUSBTestBtn.setEnabled(mUSBTestModule.isTestReady());

        // Virtual MIDI
        mVirtInputDeviceLbl.setText(mVirtTestModule.getInputName());
        mVirtOutputDeviceLbl.setText(mVirtTestModule.getOutputName());
        mVirtTestBtn.setEnabled(mVirtTestModule.isTestReady());

        // Bluetooth
        mBTInputDeviceLbl.setText(mBTTestModule.getInputName());
        mBTOutputDeviceLbl.setText(mBTTestModule.getOutputName());
        mBTTestBtn.setEnabled(mBTTestModule.isTestReady() && mUSBTestModule.isTestReady());
    }

    private boolean calcTestPassed() {
        boolean hasPassed = false;
        if (!mHasMIDI) {
            // if it doesn't report MIDI support, then it doesn't have to pass the other tests.
            hasPassed = true;
        } else {
            hasPassed = mUSBTestModule.hasTestPassed() &&
                    mVirtTestModule.hasTestPassed() &&
                    mBTTestModule.hasTestPassed();
        }

        getPassButton().setEnabled(hasPassed);
        return hasPassed;
    }

    private void scanMidiDevices() {
        if (DEBUG) {
            Log.i(TAG, "scanMidiDevices()....");
        }

        MidiDeviceInfo[] devInfos = mMidiManager.getDevices();
        mUSBTestModule.scanDevices(devInfos);
        mVirtTestModule.scanDevices(devInfos);
        mBTTestModule.scanDevices(devInfos);

        showConnectedMIDIPeripheral();
    }

    //
    // UI Updaters
    //
    private void enableTestButtons(boolean enable) {
        if (DEBUG) {
            Log.i(TAG, "enableTestButtons" + enable + ")");
        }

        runOnUiThread(new Runnable() {
            public void run() {
                if (enable) {
                    // remember, a given test might not be enabled, so we can't just enable
                    // all of the buttons
                    showConnectedMIDIPeripheral();
                } else {
                    mUSBTestBtn.setEnabled(enable);
                    mVirtTestBtn.setEnabled(enable);
                    mBTTestBtn.setEnabled(enable);
                }
            }
        });
    }

    private void showUSBTestStatus() {
        mUSBTestStatusTxt.setText(mUSBTestModule.getTestStatusString());
    }

    private void showVirtTestStatus() {
        mVirtTestStatusTxt.setText(mVirtTestModule.getTestStatusString());
    }

    private void showBTTestStatus() {
        mBTTestStatusTxt.setText(mBTTestModule.getTestStatusString());
    }

    // Need this to update UI from MIDI read thread
    public void updateTestStateUI() {
        runOnUiThread(new Runnable() {
            public void run() {
                calcTestPassed();
                showUSBTestStatus();
                showVirtTestStatus();
                showBTTestStatus();
            }
        });
    }

    class MidiServiceConnection implements ServiceConnection {
        private static final String TAG = "MidiServiceConnection";
        @Override
        public void  onServiceConnected(ComponentName name, IBinder service) {
            if (DEBUG) {
                Log.i(TAG, "MidiServiceConnection.onServiceConnected()");
            }
            scanMidiDevices();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            if (DEBUG) {
                Log.i(TAG, "MidiServiceConnection.onServiceDisconnected()");
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (DEBUG) {
            Log.i(TAG, "---- onCreate()");
        }

        setContentView(R.layout.midi_activity);

        // Standard PassFailButtons.Activity initialization
        setPassFailButtonClickListeners();
        setInfoResources(R.string.midi_test, R.string.midi_info, -1);

        // May as well calculate this right off the bat.
        mHasMIDI = hasMIDI();
        ((TextView)findViewById(R.id.midiHasMIDILbl)).setText("" + mHasMIDI);

        // USB Test Widgets
        mUSBIInputDeviceLbl = (TextView)findViewById(R.id.midiUSBInputLbl);
        mUSBOutputDeviceLbl = (TextView)findViewById(R.id.midiUSBOutputLbl);
        mUSBTestBtn = (Button)findViewById(R.id.midiTestUSBInterfaceBtn);
        mUSBTestBtn.setOnClickListener(this);
        mUSBTestStatusTxt = (TextView)findViewById(R.id.midiUSBTestStatusLbl);

        // Virtual MIDI Test Widgets
        mVirtInputDeviceLbl = (TextView)findViewById(R.id.midiVirtInputLbl);
        mVirtOutputDeviceLbl = (TextView)findViewById(R.id.midiVirtOutputLbl);
        mVirtTestBtn = (Button)findViewById(R.id.midiTestVirtInterfaceBtn);
        mVirtTestBtn.setOnClickListener(this);
        mVirtTestStatusTxt = (TextView)findViewById(R.id.midiVirtTestStatusLbl);

        // Bluetooth MIDI Test Widgets
        mBTInputDeviceLbl = (TextView)findViewById(R.id.midiBTInputLbl);
        mBTOutputDeviceLbl = (TextView)findViewById(R.id.midiBTOutputLbl);
        mBTTestBtn = (Button)findViewById(R.id.midiTestBTInterfaceBtn);
        mBTTestBtn.setOnClickListener(this);
        mBTTestStatusTxt = (TextView)findViewById(R.id.midiBTTestStatusLbl);

        // Setup Test Modules
        mUSBTestModule = new MidiTestModule(MidiDeviceInfo.TYPE_USB);
        mVirtTestModule = new MidiTestModule(MidiDeviceInfo.TYPE_VIRTUAL);
        mBTTestModule = new BTMidiTestModule();

        // Init MIDI Stuff
        mMidiManager = (MidiManager) getSystemService(Context.MIDI_SERVICE);

        mMidiServiceIntent = new Intent(this, MidiEchoTestService.class);

        // Initial MIDI Device Scan
        scanMidiDevices();

        // Plug in device connect/disconnect callback
        mMidiManager.registerDeviceCallback(new MidiDeviceCallback(), new Handler(getMainLooper()));
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (DEBUG) {
            Log.i(TAG, "---- Loading Virtual MIDI Service ...");
        }
        mMidiServiceConnection = new MidiServiceConnection();
        boolean isBound =
            bindService(mMidiServiceIntent,  mMidiServiceConnection,  Context.BIND_AUTO_CREATE);
        if (DEBUG) {
            Log.i(TAG, "---- Virtual MIDI Service loaded: " + isBound);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (DEBUG) {
            Log.i(TAG, "---- onPause()");
        }

        unbindService(mMidiServiceConnection);
        mMidiServiceConnection = null;
    }

    /**
     * Callback class for MIDI device connect/disconnect.
     */
    private class MidiDeviceCallback extends MidiManager.DeviceCallback {
        private static final String TAG = "MidiDeviceCallback";

        @Override
        public void onDeviceAdded(MidiDeviceInfo device) {
            scanMidiDevices();
        }

        @Override
        public void onDeviceRemoved(MidiDeviceInfo device) {
            scanMidiDevices();
        }
    } /* class MidiDeviceCallback */

    //
    // View.OnClickListener Override - Handles button clicks
    //
    @Override
    public void onClick(View view) {
        switch (view.getId()) {
        case R.id.midiTestUSBInterfaceBtn:
            mUSBTestModule.startLoopbackTest();
            break;

        case R.id.midiTestVirtInterfaceBtn:
            mVirtTestModule.startLoopbackTest();
            break;

        case R.id.midiTestBTInterfaceBtn:
            mBTTestModule.startLoopbackTest();
            break;

        default:
            assert false : "Unhandled button click";
        }
    }

    /**
     * A class to hold the MidiDeviceInfo and ports objects associated with a MIDI I/O peripheral.
     */
    private class MidiIODevice {
        private static final String TAG = "MidiIODevice";

        private final int mDeviceType;

        public MidiDeviceInfo mSendDevInfo;
        public MidiDeviceInfo mReceiveDevInfo;

        public MidiInputPort   mSendPort;
        public MidiOutputPort  mReceivePort;

        public MidiIODevice(int deviceType) {
            mDeviceType = deviceType;
        }

        public void scanDevices(MidiDeviceInfo[] devInfos) {
            if (DEBUG) {
                Log.i(TAG, "---- scanDevices() typeID: " + mDeviceType);
            }
            mSendDevInfo = null;
            mReceiveDevInfo = null;
            mSendPort = null;
            mReceivePort = null;

            for(MidiDeviceInfo devInfo : devInfos) {
                // Inputs?
                int numInPorts = devInfo.getInputPortCount();
                if (numInPorts <= 0) {
                    continue; // none?
                }
                if (devInfo.getType() == mDeviceType && mSendDevInfo == null) {
                    mSendDevInfo = devInfo;
                }

                // Outputs?
                int numOutPorts = devInfo.getOutputPortCount();
                if (numOutPorts <= 0) {
                    continue; // none?
                }
                if (devInfo.getType() == mDeviceType && mReceiveDevInfo == null) {
                    mReceiveDevInfo = devInfo;
                }

                if (mSendDevInfo != null && mReceiveDevInfo != null) {
                    break;  // we have an in and out device, so we can stop scanning
                }
            }

            if (DEBUG) {
                if (mSendDevInfo != null) {
                    Log.i(TAG, "---- mSendDevInfo: " + mSendDevInfo);
                }
                if (mReceiveDevInfo != null) {
                    Log.i(TAG, "---- mReceiveDevInfo: " + mReceiveDevInfo);
                }
            }
        }

        protected void openPorts(MidiDevice device, MidiReceiver receiver) {
            if (DEBUG) {
                Log.i(TAG, "---- openPorts()");
            }
            MidiDeviceInfo deviceInfo = device.getInfo();
            int numOutputs = deviceInfo.getOutputPortCount();
            if (numOutputs > 0) {
                mReceivePort = device.openOutputPort(0);
                mReceivePort.connect(receiver);
            }

            int numInputs = deviceInfo.getInputPortCount();
            if (numInputs != 0) {
                mSendPort = device.openInputPort(0);
            }
        }

        public void closePorts() {
            if (DEBUG) {
                Log.i(TAG, "---- closePorts()");
            }
            try {
                if (mSendPort != null) {
                    mSendPort.close();
                    mSendPort = null;
                }
                if (mReceivePort != null) {
                    mReceivePort.close();
                    mReceivePort = null;
                }
            } catch (IOException ex) {
                Log.e(TAG, "IOException Closing MIDI ports: " + ex);
            }
        }

        public String getInputName() {
            if (mReceiveDevInfo != null) {
                return mDeviceType == MidiDeviceInfo.TYPE_VIRTUAL
                        ? "Virtual MIDI Device"
                        : mReceiveDevInfo.getProperties().getString(MidiDeviceInfo.PROPERTY_NAME);
            } else {
                return "";
            }
        }

        public String getOutputName() {
            if (mSendDevInfo != null) {
                return mDeviceType == MidiDeviceInfo.TYPE_VIRTUAL
                        ? "Virtual MIDI Device"
                        : mSendDevInfo.getProperties().getString(MidiDeviceInfo.PROPERTY_NAME);
            } else {
                return "";
            }
        }
    }   /* class MidiIODevice */

    //
    // MIDI Messages
    //
    public static final int kMIDICmd_KeyDown = 9;
    public static final int kMIDICmd_KeyUp = 8;
    public static final int kMIDICmd_PolyPress = 10;
    public static final int kMIDICmd_Control = 11;
    public static final int kMIDICmd_ProgramChange = 12;
    public static final int kMIDICmd_ChannelPress = 13;
    public static final int kMIDICmd_PitchWheel = 14;
    public static final int kMIDICmd_SysEx = 15;
    public static final int kMIDICmd_EndOfSysEx = 0b11110111;

    private class TestMessage {
        public byte[]   mMsgBytes;

        public boolean matches(byte[] msg, int offset, int count) {
            // Length
            if (DEBUG) {
                Log.i(TAG, "  count [" + count + " : " + mMsgBytes.length + "]");
            }
            if (count != mMsgBytes.length) {
                return false;
            }

            // Data
            for(int index = 0; index < count; index++) {
                if (DEBUG) {
                    Log.i(TAG, "  [" + msg[offset + index] + " : " + mMsgBytes[index] + "]");
                }
                if (msg[offset + index] != mMsgBytes[index]) {
                    return false;
                }
            }
            return true;
        }
    } /* class TestMessage */

    private static byte makeMIDICmd(int cmd, int channel) {
        return (byte)((cmd << 4) | (channel & 0x0F));
    }

    /**
     * A class to control and represent the state of a given test.
     * It hold the data needed for IO, and the logic for sending, receiving and matching
     * the MIDI data stream.
     */
    private class MidiTestModule {
        private static final String TAG = "MidiTestModule";

        // Test Peripheral
        MidiIODevice mIODevice;

        // Test Status
        protected static final int TESTSTATUS_NOTRUN = 0;
        protected static final int TESTSTATUS_PASSED = 1;
        protected static final int TESTSTATUS_FAILED_MISMATCH = 2;
        protected static final int TESTSTATUS_FAILED_TIMEOUT = 3;

        protected int mTestStatus = TESTSTATUS_NOTRUN;
        protected boolean mTestMismatched;

        // Test Data
        // - The set of messages to send
        private TestMessage[] mTestMessages;

        // - The stream of message data to walk through when MIDI data is received.
        private byte[] mMIDIDataStream;
        private int mReceiveStreamPos;

        public MidiTestModule(int deviceType) {
            mIODevice = new MidiIODevice(deviceType);
            setupTestMessages();
        }

        // UI Helper
        public String getTestStatusString() {
            Resources appResources = getApplicationContext().getResources();
            switch (mTestStatus) {
            case TESTSTATUS_NOTRUN:
                return appResources.getString(R.string.midiNotRunLbl);

            case TESTSTATUS_PASSED:
                return appResources.getString(R.string.midiPassedLbl);

            case TESTSTATUS_FAILED_MISMATCH:
                return appResources.getString(R.string.midiFailedMismatchLbl);

            case TESTSTATUS_FAILED_TIMEOUT:
            {
                String messageStr = appResources.getString(R.string.midiFailedTimeoutLbl);
                return messageStr + " @" + mReceiveStreamPos;
            }

            default:
                return "Unknown Test Status.";
            }
        }

        public void scanDevices(MidiDeviceInfo[] devInfos) {
            mIODevice.scanDevices(devInfos);
        }

        public void showTimeoutMessage() {
            runOnUiThread(new Runnable() {
                public void run() {
                    synchronized (mTestLock) {
                        if (mTestRunning) {
                            if (DEBUG) {
                                Log.i(TAG, "---- Test Failed - TIMEOUT");
                            }
                            mTestStatus = TESTSTATUS_FAILED_TIMEOUT;
                            updateTestStateUI();
                        }
                    }
                }
            });
        }

        public void startLoopbackTest() {
            synchronized (mTestLock) {
                mTestRunning = true;
                enableTestButtons(false);
            }

            if (DEBUG) {
                Log.i(TAG, "---- startLoopbackTest()");
            }

            mTestStatus = TESTSTATUS_NOTRUN;
            mTestMismatched = false;
            mReceiveStreamPos = 0;

            // These might be left open due to a failing, previous test
            // so just to be sure...
            closePorts();

            if (mIODevice.mSendDevInfo != null) {
                mMidiManager.openDevice(mIODevice.mSendDevInfo, new TestModuleOpenListener(), null);
            }

            // Start the timeout timer
            TimerTask task = new TimerTask() {
                @Override
                public void run() {
                    synchronized (mTestLock) {
                        if (mTestRunning) {
                            // Timeout
                            showTimeoutMessage();
                            enableTestButtons(true);
                        }
                    }
                }
            };
            mTimeoutTimer.schedule(task, TEST_TIMEOUT_MS);
        }

        protected void openPorts(MidiDevice device) {
            mIODevice.openPorts(device, new MidiMatchingReceiver());
        }

        protected void closePorts() {
            mIODevice.closePorts();
        }

        public String getInputName() {
            return mIODevice.getInputName();
        }

        public String getOutputName() {
            return mIODevice.getOutputName();
        }

        public boolean isTestReady() {
            return mIODevice.mReceiveDevInfo != null && mIODevice.mSendDevInfo != null;
        }

        public boolean hasTestPassed() {
            return mTestStatus == TESTSTATUS_PASSED;
        }

        // A little explanation here... It seems reasonable to send complete MIDI messages, i.e.
        // as a set of discrete pakages.
        // However the looped-back data may not be delivered in message-size packets, so it makes more
        // sense to look at that as a stream of bytes.
        // So we build a set of messages to send, and then create the equivalent stream of bytes
        // from that to match against when received back in from the looped-back device.
        private void setupTestMessages() {
            int NUM_TEST_MESSAGES = 3;
            mTestMessages = new TestMessage[NUM_TEST_MESSAGES];

            //
            // Set up any set of messages you want
            // Except for the command IDs, the data values are purely arbitrary and meaningless
            // outside of being matched.
            // KeyDown
            mTestMessages[0] = new TestMessage();
            mTestMessages[0].mMsgBytes = new byte[]{makeMIDICmd(kMIDICmd_KeyDown, 0), 64, 12};

            // KeyUp
            mTestMessages[1] = new TestMessage();
            mTestMessages[1].mMsgBytes = new byte[]{makeMIDICmd(kMIDICmd_KeyDown, 0), 64, 45};

            // SysEx
            // NOTE: A sysex on the MT-BT01 seems to top out at sometimes as low as 40 bytes.
            // It is not clear, but needs more research. For now choose a conservative size.
            int sysExSize = 32;
            byte[] sysExMsg = new byte[sysExSize];
            sysExMsg[0] = makeMIDICmd(kMIDICmd_SysEx, 0);
            for(int index = 1; index < sysExSize-1; index++) {
                sysExMsg[index] = (byte)index;
            }
            sysExMsg[sysExSize-1] = (byte)kMIDICmd_EndOfSysEx;
            mTestMessages[2] = new TestMessage();
            mTestMessages[2].mMsgBytes = sysExMsg;

            //
            // Now build the stream to match against
            //
            int streamSize = 0;
            for (int msgIndex = 0; msgIndex < mTestMessages.length; msgIndex++) {
                streamSize += mTestMessages[msgIndex].mMsgBytes.length;
            }

            mMIDIDataStream = new byte[streamSize];

            int offset = 0;
            for (int msgIndex = 0; msgIndex < mTestMessages.length; msgIndex++) {
                int numBytes = mTestMessages[msgIndex].mMsgBytes.length;
                System.arraycopy(mTestMessages[msgIndex].mMsgBytes, 0,
                        mMIDIDataStream, offset, numBytes);
                offset += numBytes;
            }
            mReceiveStreamPos = 0;
        }

        /**
         * Compares the supplied bytes against the sent message stream at the current postion
         * and advances the stream position.
         */
        private boolean matchStream(byte[] bytes, int offset, int count) {
            if (DEBUG) {
                Log.i(TAG, "---- matchStream() offset:" + offset + " count:" + count);
            }
            boolean matches = true;

            for (int index = 0; index < count; index++) {
                if (bytes[offset + index] != mMIDIDataStream[mReceiveStreamPos]) {
                    matches = false;
                    if (DEBUG) {
                        Log.i(TAG, "---- mismatch @" + index + " [" + bytes[offset + index] +
                                " : " + mMIDIDataStream[mReceiveStreamPos] + "]");
                    }
                }
                mReceiveStreamPos++;
            }

            if (DEBUG) {
                Log.i(TAG, "  returns:" + matches);
            }
            return matches;
        }

        /**
         * Writes out the list of MIDI messages to the output port.
         */
        private void sendMessages() {
            if (DEBUG) {
                Log.i(TAG, "---- sendMessages()...");
            }
            int totalSent = 0;
            if (mIODevice.mSendPort != null) {
                try {
                    for(TestMessage msg : mTestMessages) {
                        mIODevice.mSendPort.send(msg.mMsgBytes, 0, msg.mMsgBytes.length);
                        totalSent += msg.mMsgBytes.length;
                    }
                } catch (IOException ex) {
                    Log.i(TAG, "---- IOException " + ex);
                }
            }
            if (DEBUG) {
                Log.i(TAG, "---- totalSent:" + totalSent);
            }
        }

        /**
         * Listens for MIDI device opens. Opens I/O ports and sends out the apriori
         * setup messages.
         */
        class TestModuleOpenListener implements MidiManager.OnDeviceOpenedListener {
            @Override
            public void onDeviceOpened(MidiDevice device) {
                if (DEBUG) {
                    Log.i(TAG, "---- onDeviceOpened()");
                }
                openPorts(device);
                sendMessages();
            }
        }

        /**
         * A MidiReceiver subclass whose job it is to monitor incomming messages
         * and match them against the stream sent by the test.
         */
        private class MidiMatchingReceiver extends MidiReceiver {
            private static final String TAG = "MidiMatchingReceiver";

            @Override
            public void onSend(byte[] msg, int offset, int count, long timestamp) throws IOException {
                if (!matchStream(msg, offset, count)) {
                    mTestMismatched = true;
                }
                if (DEBUG) {
                    Log.i(TAG, "onSend() @" + mReceiveStreamPos);
                }
                if (mReceiveStreamPos == mMIDIDataStream.length) {
                    synchronized (mTestLock) {
                        mTestRunning = false;
                    }

                    if (DEBUG) {
                        Log.i(TAG, "---- Test Complete");
                    }
                    // defer closing the ports to outside of this callback.
                    new Thread(new Runnable() {
                        public void run() {
                            mTestStatus = mTestMismatched
                                    ? TESTSTATUS_FAILED_MISMATCH : TESTSTATUS_PASSED;
                            Log.i(TAG, "---- mTestStatus:" + mTestStatus);
                            closePorts();
                        }
                    }).start();

                    enableTestButtons(true);
                    updateTestStateUI();
                }
            }
        } /* class MidiMatchingReceiver */
    } /* class MidiTestModule */

    /**
     * Test Module for Bluetooth Loopback.
     * This is a specialization of MidiTestModule (which has the connections for the BL device
     * itself) with and added MidiIODevice object for the USB audio device which does the
     * "looping back".
     */
    private class BTMidiTestModule extends MidiTestModule {
        private static final String TAG = "BTMidiTestModule";
        private MidiIODevice mUSBLoopbackDevice = new MidiIODevice(MidiDeviceInfo.TYPE_USB);

        public BTMidiTestModule() {
            super(MidiDeviceInfo.TYPE_BLUETOOTH );
        }

        @Override
        public void scanDevices(MidiDeviceInfo[] devInfos) {
            // (normal) Scan for BT MIDI device
            super.scanDevices(devInfos);
            // Find a USB Loopback Device
            mUSBLoopbackDevice.scanDevices(devInfos);
        }

        private void openUSBEchoDevice(MidiDevice device) {
            MidiDeviceInfo deviceInfo = device.getInfo();
            int numOutputs = deviceInfo.getOutputPortCount();
            if (numOutputs > 0) {
                mUSBLoopbackDevice.mReceivePort = device.openOutputPort(0);
                mUSBLoopbackDevice.mReceivePort.connect(new USBMidiEchoReceiver());
            }

            int numInputs = deviceInfo.getInputPortCount();
            if (numInputs != 0) {
                mUSBLoopbackDevice.mSendPort = device.openInputPort(0);
            }
        }

        public void startLoopbackTest() {
            if (DEBUG) {
                Log.i(TAG, "---- startLoopbackTest()");
            }
            // Setup the USB Loopback Device
            mUSBLoopbackDevice.closePorts();

            if (mIODevice.mSendDevInfo != null) {
                mMidiManager.openDevice(
                        mUSBLoopbackDevice.mSendDevInfo, new USBLoopbackOpenListener(), null);
            }

            // Now start the test as usual
            super.startLoopbackTest();
        }

        /**
         * We need this OnDeviceOpenedListener to open the USB-Loopback device
         */
        private class USBLoopbackOpenListener implements MidiManager.OnDeviceOpenedListener {
            @Override
            public void onDeviceOpened(MidiDevice device) {
                if (DEBUG) {
                    Log.i("USBLoopbackOpenListener", "---- onDeviceOpened()");
                }
                mUSBLoopbackDevice.openPorts(device, new USBMidiEchoReceiver());
            }
        } /* class USBLoopbackOpenListener */

        /**
         * MidiReceiver subclass for BlueTooth Loopback Test
         *
         * This class receives bytes from the USB Interface (presumably coming from the
         * Bluetooth MIDI peripheral) and echoes them back out (presumably to the Bluetooth
         * MIDI peripheral).
         */
        private class USBMidiEchoReceiver extends MidiReceiver {
            private int mTotalBytesEchoed;

            @Override
            public void onSend(byte[] msg, int offset, int count, long timestamp) throws IOException {
                mTotalBytesEchoed += count;
                if (DEBUG) {
                    Log.i(TAG, "---- USBMidiEchoReceiver.onSend() count:" + count +
                            " total:" + mTotalBytesEchoed);
                }
                mUSBLoopbackDevice.mSendPort.onSend(msg, offset, count, timestamp);
            }
        } /* class USBMidiEchoReceiver */
    } /* class BTMidiTestModule */
} /* class MidiActivity */
