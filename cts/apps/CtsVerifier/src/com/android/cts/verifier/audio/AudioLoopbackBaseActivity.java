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

import android.app.AlertDialog;

import android.content.Context;

import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaRecorder;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;

import android.util.Log;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import android.widget.Button;
import android.widget.TextView;
import android.widget.ProgressBar;
import android.widget.SeekBar;

import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

/**
 * Base class for testing activitiees that require audio loopback hardware..
 */
public class AudioLoopbackBaseActivity extends PassFailButtons.Activity {
    private static final String TAG = "AudioLoopbackActivity";

    protected AudioManager mAudioManager;

    // UI
    OnBtnClickListener mBtnClickListener = new OnBtnClickListener();

    Button mLoopbackPortYesBtn;
    Button mLoopbackPortNoBtn;

    TextView mInputDeviceTxt;
    TextView mOutputDeviceTxt;

    TextView mAudioLevelText;
    SeekBar mAudioLevelSeekbar;

    TextView mResultText;
    ProgressBar mProgressBar;
    int mMaxLevel;

    // Peripheral(s)
    boolean mIsPeripheralAttached;  // CDD ProAudio section C-1-3
    AudioDeviceInfo mOutputDevInfo;
    AudioDeviceInfo mInputDevInfo;

    // Loopback Logic
    NativeAnalyzerThread mNativeAnalyzerThread = null;

    protected double mLatencyMillis = 0.0;
    protected double mConfidence = 0.0;

    protected static final double CONFIDENCE_THRESHOLD = 0.6;
    protected static final double PROAUDIO_LATENCY_MS_LIMIT = 20.0;

    // The audio stream callback threads should stop and close
    // in less than a few hundred msec. This is a generous timeout value.
    private static final int STOP_TEST_TIMEOUT_MSEC = 2 * 1000;

    //
    // Common UI Handling
    //
    void enableLayout(int layoutId, boolean enable) {
        ViewGroup group = (ViewGroup)findViewById(layoutId);
        for (int i = 0; i < group.getChildCount(); i++) {
            group.getChildAt(i).setEnabled(enable);
        }
    }

    private void connectLoopbackUI() {
        // Has Loopback - Yes
        mLoopbackPortYesBtn = (Button)findViewById(R.id.loopback_tests_yes_btn);
        mLoopbackPortYesBtn.setOnClickListener(mBtnClickListener);

        // Has Looback - No
        mLoopbackPortNoBtn = (Button)findViewById(R.id.loopback_tests_no_btn);
        mLoopbackPortNoBtn.setOnClickListener(mBtnClickListener);

        // Connected Device
        mInputDeviceTxt = ((TextView)findViewById(R.id.audioLoopbackInputLbl));
        mOutputDeviceTxt = ((TextView)findViewById(R.id.audioLoopbackOutputLbl));

        // Loopback Info
        findViewById(R.id.loopback_tests_info_btn).setOnClickListener(mBtnClickListener);

        mAudioLevelText = (TextView)findViewById(R.id.audio_loopback_level_text);
        mAudioLevelSeekbar = (SeekBar)findViewById(R.id.audio_loopback_level_seekbar);
        mMaxLevel = mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        mAudioLevelSeekbar.setMax(mMaxLevel);
        mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, (int)(0.7 * mMaxLevel), 0);
        refreshLevel();

        mAudioLevelSeekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC,
                        progress, 0);
                refreshLevel();
                Log.i(TAG,"Changed stream volume to: " + progress);
            }
        });

        mResultText = (TextView)findViewById(R.id.audio_loopback_results_text);
        mProgressBar = (ProgressBar)findViewById(R.id.audio_loopback_progress_bar);
        showWait(false);

        enableTestUI(false);
    }

    //
    // Peripheral Connection Logic
    //
    protected void scanPeripheralList(AudioDeviceInfo[] devices) {
        // CDD Section C-1-3: USB port, host-mode support

        // Can't just use the first record because then we will only get
        // Source OR sink, not both even on devices that are both.
        mOutputDevInfo = null;
        mInputDevInfo = null;

        // Any valid peripherals
        // Do we leave in the Headset test to support a USB-Dongle?
        for (AudioDeviceInfo devInfo : devices) {
            if (devInfo.getType() == AudioDeviceInfo.TYPE_USB_DEVICE ||     // USB Peripheral
                    devInfo.getType() == AudioDeviceInfo.TYPE_USB_HEADSET ||    // USB dongle+LBPlug
                    devInfo.getType() == AudioDeviceInfo.TYPE_WIRED_HEADSET || // Loopback Plug?
                    devInfo.getType() == AudioDeviceInfo.TYPE_AUX_LINE) { // Aux-cable loopback?
                if (devInfo.isSink()) {
                    mOutputDevInfo = devInfo;
                }
                if (devInfo.isSource()) {
                    mInputDevInfo = devInfo;
                }
            }  else {
                handleDeviceConnection(devInfo);
            }
        }

        mIsPeripheralAttached = mOutputDevInfo != null || mInputDevInfo != null;
        showConnectedAudioPeripheral();

    }

    protected void handleDeviceConnection(AudioDeviceInfo deviceInfo) {
        // NOP
    }

    private class ConnectListener extends AudioDeviceCallback {
        /*package*/ ConnectListener() {}

        //
        // AudioDevicesManager.OnDeviceConnectionListener
        //
        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            scanPeripheralList(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));
        }

        @Override
        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            scanPeripheralList(mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL));
        }
    }

    protected void showConnectedAudioPeripheral() {
        mInputDeviceTxt.setText(
                mInputDevInfo != null ? mInputDevInfo.getProductName().toString()
                        : "");
        mOutputDeviceTxt.setText(
                mOutputDevInfo != null ? mOutputDevInfo.getProductName().toString()
                        : "");
    }

    /**
     * refresh Audio Level seekbar and text
     */
    private void refreshLevel() {
        int currentLevel = mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC);
        mAudioLevelSeekbar.setProgress(currentLevel);

        String levelText = String.format("%s: %d/%d",
                getResources().getString(R.string.audio_loopback_level_text),
                currentLevel, mMaxLevel);
        mAudioLevelText.setText(levelText);
    }

    private void showLoopbackInfoDialog() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.loopback_dlg_caption)
                .setMessage(R.string.loopback_dlg_text)
                .setPositiveButton(R.string.audio_general_ok, null)
                .show();
    }

    //
    // show active progress bar
    //
    protected void showWait(boolean show) {
        if (show) {
            mProgressBar.setVisibility(View.VISIBLE) ;
        } else {
            mProgressBar.setVisibility(View.INVISIBLE) ;
        }
    }


    private class OnBtnClickListener implements OnClickListener {
        @Override
        public void onClick(View v) {
            switch (v.getId()) {
                case R.id.loopback_tests_yes_btn:
                    Log.i(TAG, "User confirms Headset Port existence");
                    recordLoopbackStatus(true);
                    mLoopbackPortYesBtn.setEnabled(false);
                    mLoopbackPortNoBtn.setEnabled(false);
                    break;

                case R.id.loopback_tests_no_btn:
                    Log.i(TAG, "User denies Headset Port existence");
                    recordLoopbackStatus(false);
                    getPassButton().setEnabled(true);
                    mLoopbackPortYesBtn.setEnabled(false);
                    mLoopbackPortNoBtn.setEnabled(false);
                    break;

                case R.id.loopback_tests_info_btn:
                    showLoopbackInfoDialog();
                    break;
            }
        }
    }

    //
    // Common loging
    //
    protected void recordTestResults() {
        ReportLog reportLog = getReportLog();
        reportLog.addValue(
                "Estimated Latency",
                mLatencyMillis,
                ResultType.LOWER_BETTER,
                ResultUnit.MS);

        reportLog.addValue(
                "Confidence",
                mConfidence,
                ResultType.HIGHER_BETTER,
                ResultUnit.NONE);

        reportLog.addValue(
                "Sample Rate",
                mNativeAnalyzerThread.getSampleRate(),
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Is Peripheral Attached",
                mIsPeripheralAttached,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        if (mIsPeripheralAttached) {
            reportLog.addValue(
                    "Input Device",
                    mInputDevInfo != null ? mInputDevInfo.getProductName().toString() : "None",
                    ResultType.NEUTRAL,
                    ResultUnit.NONE);

            reportLog.addValue(
                    "Ouput Device",
                    mOutputDevInfo != null ? mOutputDevInfo.getProductName().toString() : "None",
                    ResultType.NEUTRAL,
                    ResultUnit.NONE);
        }
    }

    //
    //  test logic
    //
    protected void startAudioTest(Handler messageHandler) {
        getPassButton().setEnabled(false);
        mLatencyMillis = 0.0;
        mConfidence = 0.0;

        mNativeAnalyzerThread = new NativeAnalyzerThread();
        if (mNativeAnalyzerThread != null) {
            mNativeAnalyzerThread.setMessageHandler(messageHandler);
            // This value matches AAUDIO_INPUT_PRESET_VOICE_RECOGNITION
            mNativeAnalyzerThread.setInputPreset(MediaRecorder.AudioSource.VOICE_RECOGNITION);
            mNativeAnalyzerThread.startTest();

            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    protected void handleTestCompletion() {
        // Make sure the test thread is finished. It should already be done.
        if (mNativeAnalyzerThread != null) {
            try {
                mNativeAnalyzerThread.stopTest(STOP_TEST_TIMEOUT_MSEC);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * handler for messages from audio thread
     */
    protected Handler mMessageHandler = new Handler() {
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            switch(msg.what) {
                case NativeAnalyzerThread.NATIVE_AUDIO_THREAD_MESSAGE_REC_STARTED:
                    Log.v(TAG,"got message native rec started!!");
                    showWait(true);
                    mResultText.setText("Test Running...");
                    break;
                case NativeAnalyzerThread.NATIVE_AUDIO_THREAD_MESSAGE_OPEN_ERROR:
                    Log.v(TAG,"got message native rec can't start!!");
                    mResultText.setText("Test Error opening streams.");
                    handleTestCompletion();
                    break;
                case NativeAnalyzerThread.NATIVE_AUDIO_THREAD_MESSAGE_REC_ERROR:
                    Log.v(TAG,"got message native rec can't start!!");
                    mResultText.setText("Test Error while recording.");
                    handleTestCompletion();
                    break;
                case NativeAnalyzerThread.NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE_ERRORS:
                    mResultText.setText("Test FAILED due to errors.");
                    handleTestCompletion();
                    break;
                case NativeAnalyzerThread.NATIVE_AUDIO_THREAD_MESSAGE_REC_COMPLETE:
                    if (mNativeAnalyzerThread != null) {
                        mLatencyMillis = mNativeAnalyzerThread.getLatencyMillis();
                        mConfidence = mNativeAnalyzerThread.getConfidence();
                    }
                    mResultText.setText(String.format(
                            "Test Finished\nLatency:%.2f ms\nConfidence: %.2f",
                            mLatencyMillis,
                            mConfidence));
                    handleTestCompletion();
                    break;
                default:
                    break;
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioManager = (AudioManager)getSystemService(AUDIO_SERVICE);

        mAudioManager.registerAudioDeviceCallback(new ConnectListener(), new Handler());

        connectLoopbackUI();
    }

    private void recordLoopbackStatus(boolean has) {
        getReportLog().addValue(
                "User reported loopback availability: ",
                has ? 1.0 : 0,
                ResultType.NEUTRAL,
                ResultUnit.NONE);
    }

    //
    // Overrides
    //
    void enableTestUI(boolean enable) {

    }
}
