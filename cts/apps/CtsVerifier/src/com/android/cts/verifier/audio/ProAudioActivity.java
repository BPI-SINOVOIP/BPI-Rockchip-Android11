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

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;

import android.os.Bundle;
import android.os.Handler;

import android.util.Log;

import android.view.View;

import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import com.android.cts.verifier.R;  // needed to access resource in CTSVerifier project namespace.

public class ProAudioActivity
        extends AudioLoopbackBaseActivity
        implements View.OnClickListener {
    private static final String TAG = ProAudioActivity.class.getName();
    private static final boolean DEBUG = false;

    // Flags
    private boolean mClaimsProAudio;
    private boolean mClaimsLowLatencyAudio;    // CDD ProAudio section C-1-1
    private boolean mClaimsMIDI;               // CDD ProAudio section C-1-4
    private boolean mClaimsUSBHostMode;        // CDD ProAudio section C-1-3
    private boolean mClaimsUSBPeripheralMode;  // CDD ProAudio section C-1-3
    private boolean mClaimsHDMI;               // CDD ProAudio section C-1-3

    AudioDeviceInfo mHDMIDeviceInfo;

    // Widgets
    TextView mHDMISupportLbl;

    CheckBox mClaimsHDMICheckBox;

    Button mRoundTripTestButton;

    // Borrowed from PassFailButtons.java
    private static final int INFO_DIALOG_ID = 1337;
    private static final String INFO_DIALOG_TITLE_ID = "infoDialogTitleId";
    private static final String INFO_DIALOG_MESSAGE_ID = "infoDialogMessageId";

    public ProAudioActivity() {
        super();
    }

    private boolean claimsProAudio() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO);
    }

    private boolean claimsLowLatencyAudio() {
        // CDD Section C-1-1: android.hardware.audio.low_latency
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
    }

    private boolean claimsMIDI() {
        // CDD Section C-1-4: android.software.midi
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI);
    }

    private boolean claimsUSBHostMode() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_USB_HOST);
    }

    private boolean claimsUSBPeripheralMode() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_USB_ACCESSORY);
    }

    // HDMI Stuff
    private boolean isHDMIValid() {
        if (mHDMIDeviceInfo == null) {
            return false;
        }

        // MUST support output in stereo and eight channels...
        boolean has2Chans = false;
        boolean has8Chans = false;
        int[] channelCounts = mHDMIDeviceInfo.getChannelCounts();
        for (int count : channelCounts) {
            if (count == 2) {
                has2Chans = true;
            } else if (count == 8) {
                has8Chans = true;
            }
        }
        if (!has2Chans || !has8Chans) {
            return false;
        }

        // at 20-bit or 24-bit depth
        boolean hasFloatEncoding = false;
        int[] encodings = mHDMIDeviceInfo.getEncodings();
        for (int encoding : encodings) {
            if (encoding == AudioFormat.ENCODING_PCM_FLOAT) {
                hasFloatEncoding = true;
                break;
            }
        }
        if (!hasFloatEncoding) {
            return false;
        }

         // and 192 kHz
        boolean has192K = false;
        int[] sampleRates = mHDMIDeviceInfo.getSampleRates();
        for (int rate : sampleRates) {
            if (rate >= 192000) {
                has192K = true;
            }
        }
        if (!has192K) {
            return false;
        }

        // without bit-depth loss or resampling (hmmmmm....).

        return true;
    }

    protected void handleDeviceConnection(AudioDeviceInfo devInfo) {
        if (devInfo.isSink() && devInfo.getType() == AudioDeviceInfo.TYPE_HDMI) {
            mHDMIDeviceInfo = devInfo;
        }

        if (mHDMIDeviceInfo != null) {
            mClaimsHDMICheckBox.setChecked(true);
            mHDMISupportLbl.setText(getResources().getString(
                    isHDMIValid() ? R.string.pass_button_text : R.string.fail_button_text));
        }
        mHDMISupportLbl.setText(getResources().getString(R.string.audio_proaudio_NA));

        calculatePass();
    }

    private void calculatePass() {
        boolean hasPassed = !mClaimsProAudio ||
                (mClaimsLowLatencyAudio && mClaimsMIDI &&
                mClaimsUSBHostMode && mClaimsUSBPeripheralMode &&
                (!mClaimsHDMI || isHDMIValid()) &&
                mOutputDevInfo != null && mInputDevInfo != null &&
                mConfidence >= CONFIDENCE_THRESHOLD && mLatencyMillis <= PROAUDIO_LATENCY_MS_LIMIT);
        getPassButton().setEnabled(hasPassed);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setContentView(R.layout.pro_audio);

        super.onCreate(savedInstanceState);

        setPassFailButtonClickListeners();
        setInfoResources(R.string.proaudio_test, R.string.proaudio_info, -1);

        mClaimsProAudio = claimsProAudio();
        ((TextView)findViewById(R.id.proAudioHasProAudioLbl)).setText("" + mClaimsProAudio);

        if (!mClaimsProAudio) {
            Bundle args = new Bundle();
            args.putInt(INFO_DIALOG_TITLE_ID, R.string.pro_audio_latency_test);
            args.putInt(INFO_DIALOG_MESSAGE_ID, R.string.audio_proaudio_nopa_message);
            showDialog(INFO_DIALOG_ID, args);
        }

        mClaimsLowLatencyAudio = claimsLowLatencyAudio();
        ((TextView)findViewById(R.id.proAudioHasLLALbl)).setText("" + mClaimsLowLatencyAudio);

        mClaimsMIDI = claimsMIDI();
        ((TextView)findViewById(R.id.proAudioHasMIDILbl)).setText("" + mClaimsMIDI);

        mClaimsUSBHostMode = claimsUSBHostMode();
        ((TextView)findViewById(R.id.proAudioMidiHasUSBHostLbl)).setText("" + mClaimsUSBHostMode);

        mClaimsUSBPeripheralMode = claimsUSBPeripheralMode();
        ((TextView)findViewById(
                R.id.proAudioMidiHasUSBPeripheralLbl)).setText("" + mClaimsUSBPeripheralMode);

        mRoundTripTestButton = (Button)findViewById(R.id.proAudio_runRoundtripBtn);
        mRoundTripTestButton.setOnClickListener(this);

        // HDMI
        mHDMISupportLbl = (TextView)findViewById(R.id.proAudioHDMISupportLbl);
        mClaimsHDMICheckBox = (CheckBox)findViewById(R.id.proAudioHasHDMICheckBox);
        mClaimsHDMICheckBox.setOnClickListener(this);

        calculatePass();
    }

    protected void startAudioTest() {
        mRoundTripTestButton.setEnabled(false);
        super.startAudioTest(mMessageHandler);
    }

    protected void handleTestCompletion() {
        super.handleTestCompletion();

        calculatePass();

        recordTestResults();

        showWait(false);
        mRoundTripTestButton.setEnabled(true);
    }

    /**
     * Store test results in log
     */
    protected void recordTestResults() {
        super.recordTestResults();

        ReportLog reportLog = getReportLog();
        reportLog.addValue(
                "Claims Pro Audio",
                mClaimsProAudio,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Claims Low-Latency Audio",
                mClaimsLowLatencyAudio,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Claims MIDI",
                mClaimsMIDI,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Claims USB Host Mode",
                mClaimsUSBHostMode,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Claims USB Peripheral Mode",
                mClaimsUSBPeripheralMode,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Claims HDMI",
                mClaimsHDMI,
                ResultType.NEUTRAL,
                ResultUnit.NONE);
    }

        @Override
    public void onClick(View view) {
        switch (view.getId()) {
        case R.id.proAudio_runRoundtripBtn:
            startAudioTest();
           break;

        case R.id.proAudioHasHDMICheckBox:
            if (mClaimsHDMICheckBox.isChecked()) {
                AlertDialog.Builder builder =
                        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
                builder.setTitle(getResources().getString(R.string.proaudio_hdmi_infotitle));
                builder.setMessage(getResources().getString(R.string.proaudio_hdmi_message));
                builder.setPositiveButton(android.R.string.yes,
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {}
                 });
                builder.setIcon(android.R.drawable.ic_dialog_alert);
                builder.show();

                mClaimsHDMI = true;
                mHDMISupportLbl.setText(getResources().getString(R.string.audio_proaudio_pending));
            } else {
                mClaimsHDMI = false;
                mHDMISupportLbl.setText(getResources().getString(R.string.audio_proaudio_NA));
            }
            break;
        }
    }
}
