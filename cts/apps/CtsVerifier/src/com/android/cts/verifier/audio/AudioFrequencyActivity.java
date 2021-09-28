/*
 * Copyright (C) 2015 The Android Open Source Project
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

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;
import com.android.cts.verifier.audio.wavelib.*;
import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import android.app.AlertDialog;
import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;

/**
 * Audio Frequency Test base activity
 */
public class AudioFrequencyActivity extends PassFailButtons.Activity {
    private static final String TAG = "AudioFrequencyActivity";
    private static final boolean DEBUG = true;

    protected Context mContext;
    protected AudioManager mAudioManager;

    protected AudioDeviceInfo mOutputDevInfo;
    protected AudioDeviceInfo mInputDevInfo;

    public int mMaxLevel = 0;

    //
    // TODO - These should be refactored into a RefMicActivity class
    // i.e. AudioFrequencyActivity <- RefMicActivity
    private OnBtnClickListener mBtnClickListener = new OnBtnClickListener();


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContext = this;

        mAudioManager = (AudioManager)getSystemService(AUDIO_SERVICE);
        mAudioManager.registerAudioDeviceCallback(new ConnectListener(), new Handler());
    }

    //
    // Common UI Handling
    protected void connectRefMicUI() {
        findViewById(R.id.refmic_tests_yes_btn).setOnClickListener(mBtnClickListener);
        findViewById(R.id.refmic_tests_no_btn).setOnClickListener(mBtnClickListener);
        findViewById(R.id.refmic_test_info_btn).setOnClickListener(mBtnClickListener);

        enableTestUI(false);
    }

    private void showRefMicInfoDialog() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.ref_mic_dlg_caption)
                .setMessage(R.string.ref_mic_dlg_text)
                .setPositiveButton(R.string.audio_general_ok, null)
                .show();
    }

    private class OnBtnClickListener implements OnClickListener {
        @Override
        public void onClick(View v) {
            switch (v.getId()) {
                case R.id.refmic_tests_yes_btn:
                    recordRefMicStatus(true);
                    enableTestUI(true);
                    // disable test button so that they will now run the test(s)
                    getPassButton().setEnabled(false);
                    break;

                case R.id.refmic_tests_no_btn:
                    recordRefMicStatus(false);
                    enableTestUI(false);
                    // Allow the user to "pass" the test.
                    getPassButton().setEnabled(true);
                    break;

                case R.id.refmic_test_info_btn:
                    showRefMicInfoDialog();
                    break;
            }
        }
    }

    private void recordRefMicStatus(boolean has) {
        getReportLog().addValue(
                "User reported ref mic availability: ",
                has ? 1.0 : 0,
                ResultType.NEUTRAL,
                ResultUnit.NONE);
    }

    //
    // Overrides
    //
    void enableTestUI(boolean enable) {

    }

    void enableLayout(int layoutId, boolean enable) {
        ViewGroup group = (ViewGroup)findViewById(layoutId);
        for (int i = 0; i < group.getChildCount(); i++) {
            group.getChildAt(i).setEnabled(enable);
        }
    }

    public void setMaxLevel() {
        mMaxLevel = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, (int)(mMaxLevel), 0);
    }

    public void setMinLevel() {
        mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, 0, 0);
    }

    public void testMaxLevel() {
        int currentLevel = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        Log.i(TAG, String.format("Max level: %d curLevel: %d", mMaxLevel, currentLevel));
        if (currentLevel != mMaxLevel) {
            new AlertDialog.Builder(this)
                .setTitle(R.string.audio_general_warning)
                .setMessage(R.string.audio_general_level_not_max)
                .setPositiveButton(R.string.audio_general_ok, null)
                .show();
        }
    }

    public int getMaxLevelForStream(int streamType) {
        return mAudioManager.getStreamMaxVolume(streamType);
    }

    public void setLevelForStream(int streamType, int level) {
        try {
            mAudioManager.setStreamVolume(streamType, level, 0);
        } catch (Exception e) {
            Log.e(TAG, "Error setting stream volume: ", e);
        }
    }

    public int getLevelForStream(int streamType) {
        return mAudioManager.getStreamVolume(streamType);
    }

    public void enableUILayout(LinearLayout layout, boolean enable) {
        for (int i = 0; i < layout.getChildCount(); i++) {
            View view = layout.getChildAt(i);
            view.setEnabled(enable);
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
        }

        @Override
        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            // Log.i(TAG, "onAudioDevicesRemoved() num:" + removedDevices.length);

            scanPeripheralList(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));
        }
    }

//    abstract public void updateConnectStatus();
}
