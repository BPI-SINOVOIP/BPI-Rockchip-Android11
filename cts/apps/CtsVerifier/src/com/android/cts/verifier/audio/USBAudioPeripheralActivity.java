/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.app.AlertDialog;
import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import com.android.cts.verifier.audio.peripheralprofile.PeripheralProfile;
import com.android.cts.verifier.audio.peripheralprofile.ProfileManager;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;  // needed to access resource in CTSVerifier project namespace.

public abstract class USBAudioPeripheralActivity extends PassFailButtons.Activity {
    private static final String TAG = "USBAudioPeripheralActivity";
    private static final boolean DEBUG = false;

    // Profile
    protected ProfileManager mProfileManager = new ProfileManager();
    protected PeripheralProfile mSelectedProfile;

    // Peripheral
    AudioManager mAudioManager;
    protected boolean mIsPeripheralAttached;
    protected AudioDeviceInfo mOutputDevInfo;
    protected AudioDeviceInfo mInputDevInfo;

    protected final boolean mIsMandatedRequired;

    // This will be overriden...
    protected  int mSystemSampleRate = 48000;

    // Widgets
    private TextView mProfileNameTx;
    private TextView mProfileDescriptionTx;

    private TextView mPeripheralNameTx;

    private OnBtnClickListener mBtnClickListener = new OnBtnClickListener();

    //
    // Common UI Handling
    //
    protected void connectUSBPeripheralUI() {
        findViewById(R.id.uap_tests_yes_btn).setOnClickListener(mBtnClickListener);
        findViewById(R.id.uap_tests_no_btn).setOnClickListener(mBtnClickListener);
        findViewById(R.id.uap_test_info_btn).setOnClickListener(mBtnClickListener);

        // Leave the default state in tact
        // enableTestUI(false);
    }

    private void showUAPInfoDialog() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.uap_mic_dlg_caption)
                .setMessage(R.string.uap_mic_dlg_text)
                .setPositiveButton(R.string.audio_general_ok, null)
                .show();
    }

    private class OnBtnClickListener implements OnClickListener {
        @Override
        public void onClick(View v) {
            switch (v.getId()) {
                case R.id.uap_tests_yes_btn:
                    recordUSBAudioStatus(true);
                    enableTestUI(true);
                    // disable test button so that they will now run the test(s)
                    getPassButton().setEnabled(false);
                    break;

                case R.id.uap_tests_no_btn:
                    recordUSBAudioStatus(false);
                    enableTestUI(false);
                    // Allow the user to "pass" the test.
                    getPassButton().setEnabled(true);
                    break;

                case R.id.uap_test_info_btn:
                    showUAPInfoDialog();
                    break;
            }
        }
    }

    private void recordUSBAudioStatus(boolean has) {
        getReportLog().addValue(
                "User reported USB Host Audio Support: ",
                has ? 1.0 : 0,
                ResultType.NEUTRAL,
                ResultUnit.NONE);
    }

    //
    // Overrides
    //
    void enableTestUI(boolean enable) {

    }

    public USBAudioPeripheralActivity(boolean mandatedRequired) {
        super();

        // determine if to show "UNSUPPORTED" if the mandated peripheral is required.
        mIsMandatedRequired = mandatedRequired;

        mProfileManager.loadProfiles();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioManager = (AudioManager)getSystemService(AUDIO_SERVICE);
        mAudioManager.registerAudioDeviceCallback(new ConnectListener(), new Handler());

        mSystemSampleRate = Integer.parseInt(
            mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
    }

    protected void connectPeripheralStatusWidgets() {
        mProfileNameTx = (TextView)findViewById(R.id.uap_profileNameTx);
        mProfileDescriptionTx =
            (TextView)findViewById(R.id.uap_profileDescriptionTx);
        mPeripheralNameTx = (TextView)findViewById(R.id.uap_peripheralNameTx);
    }

    private void showProfileStatus() {
        if (DEBUG) {
            Log.d(TAG, "showProfileStatus()" + (mSelectedProfile != null));
        }
        if (mSelectedProfile != null) {
            mProfileNameTx.setText(mSelectedProfile.getName());
            mProfileDescriptionTx.setText(mSelectedProfile.getDescription());
        } else {
            mProfileNameTx.setText("");
            mProfileDescriptionTx.setText("");
        }
    }

    private void showPeripheralStatus() {
        if (mIsPeripheralAttached) {
            String productName = "";
            if (mOutputDevInfo != null) {
                productName = mOutputDevInfo.getProductName().toString();
            } else if (mInputDevInfo != null) {
                productName = mInputDevInfo.getProductName().toString();
            }
            String ctrlText;
            if (mSelectedProfile == null && mIsMandatedRequired) {
                ctrlText = productName + " - UNSUPPORTED";
            } else {
                ctrlText = productName;
            }
            mPeripheralNameTx.setText(ctrlText);
        } else {
            mPeripheralNameTx.setText("Disconnected");
        }
    }

    private void scanPeripheralList(AudioDeviceInfo[] devices) {
        // Can't just use the first record because then we will only get
        // Source OR sink, not both even on devices that are both.
        mOutputDevInfo = null;
        mInputDevInfo = null;

        // Any valid peripherals
        for(AudioDeviceInfo devInfo : devices) {
            if (devInfo.getType() == AudioDeviceInfo.TYPE_USB_DEVICE ||
                devInfo.getType() == AudioDeviceInfo.TYPE_USB_HEADSET) {
                if (devInfo.isSink()) {
                    mOutputDevInfo = devInfo;
                }
                if (devInfo.isSource()) {
                    mInputDevInfo = devInfo;
                }
            }
        }
        mIsPeripheralAttached = mOutputDevInfo != null || mInputDevInfo != null;
        if (DEBUG) {
            Log.d(TAG, "mIsPeripheralAttached: " + mIsPeripheralAttached);
        }

        // any associated profiles?
        if (mIsPeripheralAttached) {
            if (mOutputDevInfo != null) {
                mSelectedProfile =
                    mProfileManager.getProfile(mOutputDevInfo.getProductName().toString());
            } else if (mInputDevInfo != null) {
                mSelectedProfile =
                    mProfileManager.getProfile(mInputDevInfo.getProductName().toString());
            }
        } else {
            mSelectedProfile = null;
        }

    }

    private class ConnectListener extends AudioDeviceCallback {
        /*package*/ ConnectListener() {}

        //
        // AudioDevicesManager.OnDeviceConnectionListener
        //
        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            // Log.i(TAG, "onAudioDevicesAdded() num:" + addedDevices.length);

            scanPeripheralList(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));

            showProfileStatus();
            showPeripheralStatus();
            updateConnectStatus();
        }

        @Override
        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            // Log.i(TAG, "onAudioDevicesRemoved() num:" + removedDevices.length);

            scanPeripheralList(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));

            showProfileStatus();
            showPeripheralStatus();
            updateConnectStatus();
        }
    }

    abstract public void updateConnectStatus();
}

