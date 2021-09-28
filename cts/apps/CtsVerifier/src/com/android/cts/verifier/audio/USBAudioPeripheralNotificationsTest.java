/*
 * Copyright (C) 2020 The Android Open Source Project
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;

import android.os.Bundle;
import android.os.Handler;

import android.util.Log;

import android.widget.TextView;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;  // needed to access resource in CTSVerifier project namespace.

@CddTest(requirement="7.8.2.2/H-2-1,H-3-1,H-4-2,H-4-3,H-4-4,H-4-5")
public class USBAudioPeripheralNotificationsTest extends PassFailButtons.Activity {
    private static final String
            TAG = USBAudioPeripheralNotificationsTest.class.getSimpleName();

    private AudioManager    mAudioManager;

    private TextView mHeadsetInName;
    private TextView mHeadsetOutName;
    private TextView mUsbDeviceInName;
    private TextView mUsbDeviceOutName;

    // private TextView mHeadsetPlugText;
    private TextView mHeadsetPlugMessage;

    // Intents
    private HeadsetPlugReceiver mHeadsetPlugReceiver;
    private boolean mPlugIntentReceived;

    // Device
    private AudioDeviceInfo mUsbHeadsetInInfo;
    private AudioDeviceInfo mUsbHeadsetOutInfo;
    private AudioDeviceInfo mUsbDeviceInInfo;
    private AudioDeviceInfo mUsbDeviceOutInfo;

    private boolean mUsbHeadsetInReceived;
    private boolean mUsbHeadsetOutReceived;
    private boolean mUsbDeviceInReceived;
    private boolean mUsbDeviceOutReceived;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.uap_notifications_layout);

        mHeadsetInName = (TextView)findViewById(R.id.uap_messages_headset_in_name);
        mHeadsetOutName = (TextView)findViewById(R.id.uap_messages_headset_out_name);

        mUsbDeviceInName = (TextView)findViewById(R.id.uap_messages_usb_device_in_name);
        mUsbDeviceOutName = (TextView)findViewById(R.id.uap_messages_usb_device__out_name);

        mHeadsetPlugMessage = (TextView)findViewById(R.id.uap_messages_plug_message);

        mAudioManager = (AudioManager)getSystemService(AUDIO_SERVICE);
        mAudioManager.registerAudioDeviceCallback(new ConnectListener(), new Handler());

        mHeadsetPlugReceiver = new HeadsetPlugReceiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_HEADSET_PLUG);
        registerReceiver(mHeadsetPlugReceiver, filter);

        setInfoResources(R.string.audio_uap_notifications_test, R.string.uapNotificationsTestInfo,
                -1);

        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);
    }

    //
    // UI
    //
    private void showConnectedDevices() {
        if (mUsbHeadsetInInfo != null) {
            mHeadsetInName.setText(
                    "Headset INPUT Connected " + mUsbHeadsetInInfo.getProductName());
        } else {
            mHeadsetInName.setText("");
        }

        if (mUsbHeadsetOutInfo != null) {
            mHeadsetOutName.setText(
                    "Headset OUTPUT Connected " + mUsbHeadsetOutInfo.getProductName());
        } else {
            mHeadsetOutName.setText("");
        }

        if (mUsbDeviceInInfo != null) {
            mUsbDeviceInName.setText(
                    "USB DEVICE INPUT Connected " + mUsbDeviceInInfo.getProductName());
        } else {
            mUsbDeviceInName.setText("");
        }

        if (mUsbDeviceOutInfo != null) {
            mUsbDeviceOutName.setText(
                    "USB DEVICE OUTPUT Connected " + mUsbDeviceOutInfo.getProductName());
        } else {
            mUsbDeviceOutName.setText("");
        }
    }

    private void reportPlugIntent(Intent intent) {
        // [ 7.8 .2.2/H-2-1] MUST broadcast Intent ACTION_HEADSET_PLUG with "microphone" extra
        // set to 0 when the USB audio terminal types 0x0302 is detected.
        // [ 7.8 .2.2/H-3-1] MUST broadcast Intent ACTION_HEADSET_PLUG with "microphone" extra
        // set to 1 when the USB audio terminal types 0x0402 is detected, they:
        mPlugIntentReceived = true;

        // state - 0 for unplugged, 1 for plugged.
        // name - Headset type, human readable string
        // microphone - 1 if headset has a microphone, 0 otherwise
        int state = intent.getIntExtra("state", -1);
        if (state != -1) {

            StringBuilder sb = new StringBuilder();
            sb.append("ACTION_HEADSET_PLUG received - " + (state == 0 ? "Unplugged" : "Plugged"));

            String name = intent.getStringExtra("name");
            if (name != null) {
                sb.append(" - " + name);
            }

            int hasMic = intent.getIntExtra("microphone", 0);
            if (hasMic == 1) {
                sb.append(" [mic]");
            }

            mHeadsetPlugMessage.setText(sb.toString());
        }

        getReportLog().addValue(
                "ACTION_HEADSET_PLUG Intent Received. State: ",
                state,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        getPassButton().setEnabled(calculatePass());
    }

    //
    // Test Status
    //
    private boolean calculatePass() {
        return mUsbHeadsetInReceived && mUsbHeadsetOutReceived &&
                mUsbDeviceInReceived && mUsbDeviceOutReceived &&
                mPlugIntentReceived;
    }

    //
    // Devices
    //
    private void scanDevices(AudioDeviceInfo[] devices) {
        mUsbHeadsetInInfo = mUsbHeadsetOutInfo =
                mUsbDeviceInInfo = mUsbDeviceOutInfo = null;

        for (AudioDeviceInfo devInfo : devices) {
            if (devInfo.getType() == AudioDeviceInfo.TYPE_USB_HEADSET) {
                if (devInfo.isSource()) {
                    // [ 7.8 .2.2/H-4-3] MUST list a device of type AudioDeviceInfo.TYPE_USB_HEADSET
                    // and role isSource() if the USB audio terminal type field is 0x0402.
                    mUsbHeadsetInReceived = true;
                    mUsbHeadsetInInfo = devInfo;
                    getReportLog().addValue(
                            "USB Headset connected - INPUT",
                            0,
                            ResultType.NEUTRAL,
                            ResultUnit.NONE);
                } else if (devInfo.isSink()) {
                    // [ 7.8 .2.2/H-4-2] MUST list a device of type AudioDeviceInfo.TYPE_USB_HEADSET
                    // and role isSink() if the USB audio terminal type field is 0x0402.
                    mUsbHeadsetOutReceived = true;
                    mUsbHeadsetOutInfo = devInfo;
                    getReportLog().addValue(
                            "USB Headset connected - OUTPUT",
                            0,
                            ResultType.NEUTRAL,
                            ResultUnit.NONE);
                }
            } else if (devInfo.getType() == AudioDeviceInfo.TYPE_USB_DEVICE) {
                if (devInfo.isSource()) {
                    // [ 7.8 .2.2/H-4-5] MUST list a device of type AudioDeviceInfo.TYPE_USB_DEVICE
                    // and role isSource() if the USB audio terminal type field is 0x604.
                    mUsbDeviceInReceived = true;
                    mUsbDeviceInInfo = devInfo;
                    getReportLog().addValue(
                            "USB Device connected - INPUT",
                            0,
                            ResultType.NEUTRAL,
                            ResultUnit.NONE);
                } else if (devInfo.isSink()) {
                    // [ 7.8 .2.2/H-4-4] MUST list a device of type AudioDeviceInfo.TYPE_USB_DEVICE
                    // and role isSink() if the USB audio terminal type field is 0x603.
                    mUsbDeviceOutReceived = true;
                    mUsbDeviceOutInfo = devInfo;
                    getReportLog().addValue(
                            "USB Headset connected - OUTPUT",
                            0,
                            ResultType.NEUTRAL,
                            ResultUnit.NONE);
                }
            }

            if (mUsbHeadsetInInfo != null &&
                    mUsbHeadsetOutInfo != null &&
                    mUsbDeviceInInfo != null &&
                    mUsbDeviceOutInfo != null) {
                break;
            }
        }


        showConnectedDevices();
        getPassButton().setEnabled(calculatePass());
    }

    private class ConnectListener extends AudioDeviceCallback {
        /*package*/ ConnectListener() {}

        //
        // AudioDevicesManager.OnDeviceConnectionListener
        //
        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            Log.i(TAG, "onAudioDevicesAdded() num:" + addedDevices.length);

            scanDevices(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));
        }

        @Override
        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            Log.i(TAG, "onAudioDevicesRemoved() num:" + removedDevices.length);

            scanDevices(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));
        }
    }

    // Intents
    private class HeadsetPlugReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            reportPlugIntent(intent);
        }
    }

}
