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

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
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
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.lang.InterruptedException;
import java.util.Timer;
import java.util.TimerTask;

import com.android.cts.verifier.audio.midilib.NativeMidiManager;

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
 * A note about the "virtual MIDI" device...
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
public class NDKMidiActivity extends PassFailButtons.Activity implements View.OnClickListener {

    private static final String TAG = "NDKMidiActivity";
    private static final boolean DEBUG = false;

    private MidiManager mMidiManager;

    // Flags
    private boolean mHasMIDI;

    // Test "Modules"
    private NDKMidiTestModule   mUSBTestModule;
    private NDKMidiTestModule   mVirtTestModule;
    private BTMidiTestModule    mBTTestModule;

    // Widgets
    private Button          mUSBTestBtn;
    private Button          mVirtTestBtn;
    private Button          mBTTestBtn;

    private TextView        mUSBIInputDeviceLbl;
    private TextView        mUSBOutputDeviceLbl;
    private TextView        mUSBTestStatusTxt;

    private TextView        mVirtInputDeviceLbl;
    private TextView        mVirtOutputDeviceLbl;
    private TextView        mVirtTestStatusTxt;

    private TextView        mBTInputDeviceLbl;
    private TextView        mBTOutputDeviceLbl;
    private TextView        mBTTestStatusTxt;

    private Intent          mMidiServiceIntent;
    private ComponentName   mMidiService;

    public NDKMidiActivity() {
        super();

        NativeMidiManager.loadNativeAPI();
        NativeMidiManager.initN();
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
        mBTTestBtn.setEnabled(mBTTestModule.isTestReady());
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
        mUSBTestModule.scanDevices(devInfos, MidiDeviceInfo.TYPE_USB);
        mVirtTestModule.scanDevices(devInfos, MidiDeviceInfo.TYPE_VIRTUAL);
        mBTTestModule.scanDevices(devInfos, MidiDeviceInfo.TYPE_BLUETOOTH);

        showConnectedMIDIPeripheral();
    }

    //
    // UI Updaters
    //
    public void enableTestButtons(boolean enable) {
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
        if (DEBUG) {
            Log.i(TAG, "updateTestStateUI()");
        }
        runOnUiThread(new Runnable() {
            public void run() {
                calcTestPassed();
                showUSBTestStatus();
                showVirtTestStatus();
                showBTTestStatus();
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (DEBUG) {
            Log.i(TAG, "---- onCreate()");
        }

        // Start MIDI Echo service
        mMidiServiceIntent = new Intent(this, MidiEchoTestService.class);
        mMidiService = startService(mMidiServiceIntent);
        if (DEBUG) {
            Log.i(TAG, "---- mMidiService instantiated:" + mMidiService);
        }

        // Setup UI
        setContentView(R.layout.ndk_midi_activity);

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

        // Init MIDI Stuff
        mMidiManager = (MidiManager) getSystemService(Context.MIDI_SERVICE);

        // Setup Test Modules
        mUSBTestModule = new NDKMidiTestModule(this, mMidiManager);
        mVirtTestModule = new NDKMidiTestModule(this, mMidiManager);
        mBTTestModule = new BTMidiTestModule(this, mMidiManager);

        // Initial MIDI Device Scan
        scanMidiDevices();

        // Plug in device connect/disconnect callback
        mMidiManager.registerDeviceCallback(new MidiDeviceCallback(), new Handler(getMainLooper()));

    }

    @Override
    protected void onPause () {
        super.onPause();
        if (DEBUG) {
            Log.i(TAG, "---- onPause()");
        }

        boolean isFound = stopService(mMidiServiceIntent);
        if (DEBUG) {
            Log.i(TAG, "---- Stop Service: " + isFound);
        }
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
     * A class to control and represent the state of a given test.
     * It hold the data needed for IO, and the logic for sending, receiving and matching
     * the MIDI data stream.
     */
    public class NDKMidiTestModule {
        private static final String TAG = "NDKMidiTestModule";
        private static final boolean DEBUG = true;

        private NDKMidiActivity     mMidiActivity;

        private MidiManager         mMidiManager;
        private NativeMidiManager   mNativeMidiManager;

        // Test State
        private final Object        mTestLock = new Object();
        private boolean             mTestRunning;

        // Timeout handling
        private static final int    TEST_TIMEOUT_MS = 1000;
        private final Timer         mTimeoutTimer = new Timer();

        // Test Peripheral
        MidiIODevice                mIODevice = new MidiIODevice();

        // Test Status
        protected static final int TESTSTATUS_NOTRUN = 0;
        protected static final int TESTSTATUS_PASSED = 1;
        protected static final int TESTSTATUS_FAILED_MISMATCH = 2;
        protected static final int TESTSTATUS_FAILED_TIMEOUT = 3;
        protected static final int TESTSTATUS_FAILED_OVERRUN = 4;
        protected static final int TESTSTATUS_FAILED_DEVICE = 5;
        protected static final int TESTSTATUS_FAILED_JNI = 6;
        protected int mTestStatus = TESTSTATUS_NOTRUN;

        /**
         * A class to hold the MidiDeviceInfo and ports objects associated
         * with a MIDI I/O peripheral.
         */
        class MidiIODevice {
            private static final String TAG = "MidiIODevice";

            public MidiDeviceInfo   mSendDevInfo;
            public MidiDeviceInfo   mReceiveDevInfo;

            public MidiInputPort    mSendPort;
            public MidiOutputPort   mReceivePort;

            public void scanDevices(MidiDeviceInfo[] devInfos, int typeID) {
                if (DEBUG) {
                    Log.i(TAG, "---- scanDevices() typeID: " + typeID);
                }

                mSendDevInfo = null;
                mReceiveDevInfo = null;
                mSendPort = null;
                mReceivePort = null;

                for (MidiDeviceInfo devInfo : devInfos) {
                    // Inputs?
                    int numInPorts = devInfo.getInputPortCount();
                    if (numInPorts <= 0) {
                        continue; // none?
                    }
                    if (devInfo.getType() == typeID && mSendDevInfo == null) {
                        mSendDevInfo = devInfo;
                    }

                    // Outputs?
                    int numOutPorts = devInfo.getOutputPortCount();
                    if (numOutPorts <= 0) {
                        continue; // none?
                    }
                    if (devInfo.getType() == typeID && mReceiveDevInfo == null) {
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
                return mReceiveDevInfo != null
                        ? mReceiveDevInfo.getProperties().getString(MidiDeviceInfo.PROPERTY_NAME)
                        : "";
            }

            public String getOutputName() {
                return mSendDevInfo != null
                        ? mSendDevInfo.getProperties().getString(MidiDeviceInfo.PROPERTY_NAME)
                        : "";
            }
        }   /* class MidiIODevice */

        public NDKMidiTestModule(NDKMidiActivity midiActivity, MidiManager midiManager) {
            mMidiActivity = midiActivity;
            mMidiManager = midiManager;
            mNativeMidiManager = new NativeMidiManager();

            // this call is just to keep the build from stripping out "endTest", because
            // it is only called from JNI.
            endTest(TESTSTATUS_NOTRUN);
        }

        // UI Helper
        public String getTestStatusString() {
            Resources appResources = mMidiActivity.getApplicationContext().getResources();
            int status;
            synchronized (mTestLock) {
                status = mTestStatus;
            }
            switch (status) {
            case TESTSTATUS_NOTRUN:
                return appResources.getString(R.string.midiNotRunLbl);

            case TESTSTATUS_PASSED:
                return appResources.getString(R.string.midiPassedLbl);

            case TESTSTATUS_FAILED_MISMATCH:
                return appResources.getString(R.string.midiFailedMismatchLbl);

            case TESTSTATUS_FAILED_TIMEOUT:
                return appResources.getString(R.string.midiFailedTimeoutLbl);

            case TESTSTATUS_FAILED_OVERRUN:
                return appResources.getString(R.string.midiFailedOverrunLbl);

            case TESTSTATUS_FAILED_DEVICE:
                return appResources.getString(R.string.midiFailedDeviceLbl);

            case TESTSTATUS_FAILED_JNI:
                return appResources.getString(R.string.midiFailedJNILbl);

            default:
                return "Unknown Test Status.";
            }
        }

        public void scanDevices(MidiDeviceInfo[] devInfos, int typeID) {
            mIODevice.scanDevices(devInfos, typeID);
        }

        public void showTimeoutMessage() {
            mMidiActivity.runOnUiThread(new Runnable() {
                public void run() {
                    synchronized (mTestLock) {
                        if (mTestRunning) {
                            if (DEBUG) {
                                Log.i(TAG, "---- Test Failed - TIMEOUT");
                            }
                            mTestStatus = TESTSTATUS_FAILED_TIMEOUT;
                            mMidiActivity.updateTestStateUI();
                        }
                    }
                }
            });
        }

        public void startLoopbackTest() {
            synchronized (mTestLock) {
                mTestRunning = true;
                mMidiActivity.enableTestButtons(false);
            }

            if (DEBUG) {
                Log.i(TAG, "---- startLoopbackTest()");
            }

            synchronized (mTestLock) {
                mTestStatus = TESTSTATUS_NOTRUN;
            }

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
                            mMidiActivity.enableTestButtons(true);
                        }
                    }
                }
            };
            mTimeoutTimer.schedule(task, TEST_TIMEOUT_MS);
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
            int status;
            synchronized (mTestLock) {
                status = mTestStatus;
            }
            return status == TESTSTATUS_PASSED;
        }

        public void endTest(int endCode) {
            synchronized (mTestLock) {
                mTestRunning = false;
                mTestStatus = endCode;
            }
            if (endCode != TESTSTATUS_NOTRUN) {
                mMidiActivity.updateTestStateUI();
                mMidiActivity.enableTestButtons(true);
            }
        }

        /**
         * Listens for MIDI device opens. Opens I/O ports and sends out the apriori
         * setup messages.
         */
        class TestModuleOpenListener implements MidiManager.OnDeviceOpenedListener {
            //
            // This is where the logical part of the test starts
            //
            @Override
            public void onDeviceOpened(MidiDevice device) {
                if (DEBUG) {
                    Log.i(TAG, "---- onDeviceOpened()");
                }
                mNativeMidiManager.startTest(NDKMidiTestModule.this, device);
            }
        }
    } /* class NDKMidiTestModule */

    /**
     * Test Module for Bluetooth Loopback.
     * This is a specialization of NDKMidiTestModule (which has the connections for the BL device
     * itself) with and added MidiIODevice object for the USB audio device which does the
     * "looping back".
     */
    private class BTMidiTestModule extends NDKMidiTestModule {
        private static final String TAG = "BTMidiTestModule";
        private MidiIODevice mUSBLoopbackDevice = new MidiIODevice();

        public BTMidiTestModule(NDKMidiActivity midiActivity, MidiManager midiManager) {
            super(midiActivity, midiManager);
        }

        @Override
        public void scanDevices(MidiDeviceInfo[] devInfos, int typeID) {
            // (normal) Scan for BT MIDI device
            super.scanDevices(devInfos, typeID);
            // Find a USB Loopback Device
            mUSBLoopbackDevice.scanDevices(devInfos, MidiDeviceInfo.TYPE_USB);
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
} /* class NDKMidiActivity */
