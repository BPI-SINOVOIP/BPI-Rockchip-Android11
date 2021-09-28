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

package com.android.midi;

import android.content.Context;
import android.content.Intent;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceService;
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiManager;
import android.media.midi.MidiReceiver;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import java.io.IOException;

/**
 * Virtual MIDI Device that copies its input to its output.
 * This is used for loop-back testing of MIDI I/O.
 *
 * Note: The application's AndroidManifest.xml should contain the following in
 * its <application> section.
 *
     <service android:name="MidiEchoTestService"
         android:permission="android.permission.BIND_MIDI_DEVICE_SERVICE">
         <intent-filter>
             <action android:name="android.media.midi.MidiDeviceService" />
         </intent-filter>
         <meta-data android:name="android.media.midi.MidiDeviceService"
            android:resource="@xml/echo_device_info" />
     </service>

 * also it must provide an xml reource file "echo_device_info.xml" containing:
     <devices>
         <device manufacturer="AndroidCTS" product="MidiEcho" tags="echo,test">
             <input-port name="input" />
             <output-port name="output" />
         </device>
     </devices>
 */

public class MidiEchoTestService extends MidiDeviceService {
    private static final String TAG = "MidiEchoTestService";
    private static final boolean DEBUG = false;

    // Other apps will write to this port.
    private MidiReceiver mInputReceiver = new MyReceiver();
    // This app will copy the data to this port.
    private MidiReceiver mOutputReceiver;
    private static MidiEchoTestService sInstance;

    // These are public so we can easily read them from CTS test.
    public int statusChangeCount;
    public boolean inputOpened;
    public int outputOpenCount;

    public static final String TEST_MANUFACTURER = "AndroidCTS";
    public static final String ECHO_PRODUCT = "MidiEcho";

    /**
     * Search through the available devices for the ECHO loop-back device.
     */
    public static MidiDeviceInfo findEchoDevice(Context context) {
        MidiManager midiManager =
                (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        MidiDeviceInfo[] infos = midiManager.getDevices();
        MidiDeviceInfo echoInfo = null;
        for (MidiDeviceInfo info : infos) {
            Bundle properties = info.getProperties();
            String manufacturer = (String) properties.get(
                    MidiDeviceInfo.PROPERTY_MANUFACTURER);

            if (TEST_MANUFACTURER.equals(manufacturer)) {
                String product = (String) properties.get(
                        MidiDeviceInfo.PROPERTY_PRODUCT);
                if (ECHO_PRODUCT.equals(product)) {
                    echoInfo = info;
                    break;
                }
            }
        }
        if (DEBUG) {
            Log.i(TAG, "MidiEchoService for " + ECHO_PRODUCT + ": " + echoInfo);
        }
        return echoInfo;
    }

    /**
     * @return A textual name for this echo service.
     */
    public static String getEchoServerName() {
        return ECHO_PRODUCT;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) {
            Log.i(TAG, "#### onCreate()");
        }
        sInstance = this;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (DEBUG) {
            Log.i(TAG, "#### onDestroy()");
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DEBUG) {
            Log.i(TAG, "#### onStartCommand()");
        }
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DEBUG) {
            Log.i(TAG, "#### onBind()");
        }
        return super.onBind(intent);
    }

    // For CTS testing, so I can read test fields.
    public static MidiEchoTestService getInstance() {
        return sInstance;
    }

    @Override
    public MidiReceiver[] onGetInputPortReceivers() {
        return new MidiReceiver[] { mInputReceiver };
    }

    class MyReceiver extends MidiReceiver {
        @Override
        public void onSend(byte[] data, int offset, int count, long timestamp)
                throws IOException {
            if (mOutputReceiver == null) {
                mOutputReceiver = getOutputPortReceivers()[0];
            }
            // Copy input to output.
            mOutputReceiver.send(data, offset, count, timestamp);
        }
    }

    @Override
    public void onDeviceStatusChanged(MidiDeviceStatus status) {
        statusChangeCount++;
        inputOpened = status.isInputPortOpen(0);
        outputOpenCount = status.getOutputPortOpenCount(0);
    }
}
