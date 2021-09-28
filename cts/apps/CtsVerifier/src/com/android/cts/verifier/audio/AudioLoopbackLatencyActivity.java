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

import android.content.Context;

import android.os.Bundle;

import android.util.Log;

import android.view.View;
import android.view.View.OnClickListener;

import android.widget.Button;
import android.widget.TextView;

import com.android.compatibility.common.util.ReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import com.android.cts.verifier.R;

/**
 * Tests Audio Device roundtrip latency by using a loopback plug.
 */
public class AudioLoopbackLatencyActivity extends AudioLoopbackBaseActivity {
    private static final String TAG = "AudioLoopbackLatencyActivity";

//    public static final int BYTES_PER_FRAME = 2;

    private int mMinBufferSizeInFrames = 0;

    OnBtnClickListener mBtnClickListener = new OnBtnClickListener();

    Button mTestButton;

    private class OnBtnClickListener implements OnClickListener {
        @Override
        public void onClick(View v) {
            switch (v.getId()) {
                case R.id.audio_loopback_test_btn:
                    Log.i(TAG, "audio loopback test");
                    startAudioTest();
                    break;
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // we need to do this first so that the layout is inplace when the super-class inits
        setContentView(R.layout.audio_loopback_latency_activity);
        super.onCreate(savedInstanceState);

        mTestButton =(Button)findViewById(R.id.audio_loopback_test_btn);
        mTestButton.setOnClickListener(mBtnClickListener);

        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);
        setInfoResources(R.string.audio_loopback_latency_test, R.string.audio_loopback_info, -1);
    }

    protected void startAudioTest() {
        mTestButton.setEnabled(false);
        super.startAudioTest(mMessageHandler);
    }

    protected void handleTestCompletion() {
        super.handleTestCompletion();

        boolean resultValid = mConfidence >= CONFIDENCE_THRESHOLD
                && mLatencyMillis > 1.0;
        getPassButton().setEnabled(resultValid);

        recordTestResults();

        showWait(false);
        mTestButton.setEnabled(true);
    }

    /**
     * Store test results in log
     */
    protected void recordTestResults() {
        super.recordTestResults();

        ReportLog reportLog = getReportLog();
        int audioLevel = mAudioLevelSeekbar.getProgress();
        reportLog.addValue(
                "Audio Level",
                audioLevel,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        reportLog.addValue(
                "Frames Buffer Size",
                mMinBufferSizeInFrames,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        Log.v(TAG,"Results Recorded");
    }
}
