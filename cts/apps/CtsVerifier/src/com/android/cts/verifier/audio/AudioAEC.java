/*
 * Copyright (C) 2019 The Android Open Source Project
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
import android.media.*;
import android.media.audiofx.AcousticEchoCanceler;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;
import com.android.cts.verifier.R;
import com.android.cts.verifier.audio.wavelib.*;

public class AudioAEC extends AudioFrequencyActivity implements View.OnClickListener {
    private static final String TAG = "AudioAEC";

    private static final int TEST_NONE = -1;
    private static final int TEST_AEC = 0;
    private static final int TEST_COUNT = 1;
    private static final float MAX_VAL = (float)(1 << 15);

    private int mCurrentTest = TEST_NONE;
    private LinearLayout mLinearLayout;
    private Button mButtonTest;
    private Button mButtonMandatoryYes;
    private Button mButtonMandatoryNo;
    private ProgressBar mProgress;
    private TextView mResultTest;
    private boolean mTestAECPassed;
    private SoundPlayerObject mSPlayer;
    private SoundRecorderObject mSRecorder;
    private AcousticEchoCanceler mAec;

    private boolean mMandatory = true;

    private final int mBlockSizeSamples = 4096;
    private final int mSamplingRate = 48000;
    private final int mSelectedRecordSource = MediaRecorder.AudioSource.VOICE_COMMUNICATION;

    private final int TEST_DURATION_MS = 8000;
    private final int SHOT_FREQUENCY_MS = 200;
    private final int CORRELATION_DURATION_MS = TEST_DURATION_MS - 3000;
    private final int SHOT_COUNT_CORRELATION = CORRELATION_DURATION_MS/SHOT_FREQUENCY_MS;
    private final int SHOT_COUNT = TEST_DURATION_MS/SHOT_FREQUENCY_MS;
    private final float MIN_RMS_DB = -60.0f; //dB
    private final float MIN_RMS_VAL = (float)Math.pow(10,(MIN_RMS_DB/20));

    private final double TEST_THRESHOLD_AEC_ON = 0.5;
    private final double TEST_THRESHOLD_AEC_OFF = 0.6;
    private RmsHelper mRMSRecorder1 = new RmsHelper(mBlockSizeSamples, SHOT_COUNT);
    private RmsHelper mRMSRecorder2 = new RmsHelper(mBlockSizeSamples, SHOT_COUNT);

    private RmsHelper mRMSPlayer1 = new RmsHelper(mBlockSizeSamples, SHOT_COUNT);
    private RmsHelper mRMSPlayer2 = new RmsHelper(mBlockSizeSamples, SHOT_COUNT);

    private Thread mTestThread;

    //RMS helpers
    public class RmsHelper {
        private double mRmsCurrent;
        public int mBlockSize;
        private int mShoutCount;
        public boolean mRunning = false;

        private short[] mAudioShortArray;

        private DspBufferDouble mRmsSnapshots;
        private int mShotIndex;

        public RmsHelper(int blockSize, int shotCount) {
            mBlockSize = blockSize;
            mShoutCount = shotCount;
            reset();
        }

        public void reset() {
            mAudioShortArray = new short[mBlockSize];
            mRmsSnapshots = new DspBufferDouble(mShoutCount);
            mShotIndex = 0;
            mRmsCurrent = 0;
            mRunning = false;
        }

        public void captureShot() {
            if (mShotIndex >= 0 && mShotIndex < mRmsSnapshots.getSize()) {
                mRmsSnapshots.setValue(mShotIndex++, mRmsCurrent);
            }
        }

        public void setRunning(boolean running) {
            mRunning = running;
        }

        public double getRmsCurrent() {
            return mRmsCurrent;
        }

        public DspBufferDouble getRmsSnapshots() {
            return mRmsSnapshots;
        }

        public boolean updateRms(PipeShort pipe, int channelCount, int channel) {
            if (mRunning) {
                int samplesAvailable = pipe.availableToRead();
                while (samplesAvailable >= mBlockSize) {
                    pipe.read(mAudioShortArray, 0, mBlockSize);

                    double rmsTempSum = 0;
                    int count = 0;
                    for (int i = channel; i < mBlockSize; i += channelCount) {
                        float value = mAudioShortArray[i] / MAX_VAL;

                        rmsTempSum += value * value;
                        count++;
                    }
                    float rms = count > 0 ? (float)Math.sqrt(rmsTempSum / count) : 0f;
                    if (rms < MIN_RMS_VAL) {
                        rms = MIN_RMS_VAL;
                    }

                    double alpha = 0.9;
                    double total_rms = rms * alpha + mRmsCurrent * (1.0f - alpha);
                    mRmsCurrent = total_rms;

                    samplesAvailable = pipe.availableToRead();
                }
                return true;
            }
            return false;
        }
    }

    //compute Acoustic Coupling Factor
    private double computeAcousticCouplingFactor(DspBufferDouble buffRmsPlayer,
                                                 DspBufferDouble buffRmsRecorder,
                                                 int firstShot, int lastShot) {
        int len = Math.min(buffRmsPlayer.getSize(), buffRmsRecorder.getSize());

        firstShot = Math.min(firstShot, 0);
        lastShot = Math.min(lastShot, len -1);

        int actualLen = lastShot - firstShot + 1;

        double maxValue = 0;
        if (actualLen > 0) {
            DspBufferDouble rmsPlayerdB = new DspBufferDouble(actualLen);
            DspBufferDouble rmsRecorderdB = new DspBufferDouble(actualLen);
            DspBufferDouble crossCorr = new DspBufferDouble(actualLen);

            for (int i = firstShot, index = 0; i <= lastShot; ++i, ++index) {
                double valPlayerdB = Math.max(20 * Math.log10(buffRmsPlayer.mData[i]), MIN_RMS_DB);
                rmsPlayerdB.setValue(index, valPlayerdB);
                double valRecorderdB = Math.max(20 * Math.log10(buffRmsRecorder.mData[i]),
                        MIN_RMS_DB);
                rmsRecorderdB.setValue(index, valRecorderdB);
            }

            //cross correlation...
            if (DspBufferMath.crossCorrelation(crossCorr, rmsPlayerdB, rmsRecorderdB) !=
                    DspBufferMath.MATH_RESULT_SUCCESS) {
                Log.v(TAG, "math error in cross correlation");
            }

            for (int i = 0; i < len; i++) {
                if (Math.abs(crossCorr.mData[i]) > maxValue) {
                    maxValue = Math.abs(crossCorr.mData[i]);
                }
            }
        }
        return maxValue;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.audio_aec_activity);

        //
        mLinearLayout = (LinearLayout)findViewById(R.id.audio_aec_test_layout);
        mButtonMandatoryYes = (Button) findViewById(R.id.audio_aec_mandatory_yes);
        mButtonMandatoryYes.setOnClickListener(this);
        mButtonMandatoryNo = (Button) findViewById(R.id.audio_aec_mandatory_no);
        mButtonMandatoryNo.setOnClickListener(this);
        enableUILayout(mLinearLayout, false);

        // Test
        mButtonTest = (Button) findViewById(R.id.audio_aec_button_test);
        mButtonTest.setOnClickListener(this);
        mProgress = (ProgressBar) findViewById(R.id.audio_aec_test_progress_bar);
        mResultTest = (TextView) findViewById(R.id.audio_aec_test_result);

        showView(mProgress, false);

        mSPlayer = new SoundPlayerObject(false, mBlockSizeSamples) {

            @Override
            public void periodicNotification(AudioTrack track) {
                int channelCount = getChannelCount();
                mRMSPlayer1.updateRms(mPipe, channelCount, 0); //Only updated if running
                mRMSPlayer2.updateRms(mPipe, channelCount, 0);
            }
        };

        mSRecorder = new SoundRecorderObject(mSamplingRate, mBlockSizeSamples,
                mSelectedRecordSource) {
            @Override
            public void periodicNotification(AudioRecord recorder) {
                mRMSRecorder1.updateRms(mPipe, 1, 0); //always 1 channel
                mRMSRecorder2.updateRms(mPipe, 1, 0);
            }
        };

        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);
        setInfoResources(R.string.audio_aec_test,
                R.string.audio_aec_info, -1);
    }

    private void showView(View v, boolean show) {
        v.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();
        switch (id) {
            case R.id.audio_aec_button_test:
                startTest();
                break;
            case R.id.audio_aec_mandatory_no:
                enableUILayout(mLinearLayout,false);
                getPassButton().setEnabled(true);
                mButtonMandatoryNo.setEnabled(false);
                mButtonMandatoryYes.setEnabled(false);
                mMandatory = false;
                Log.v(TAG,"AEC marked as NOT mandatory");
                break;
            case R.id.audio_aec_mandatory_yes:
                enableUILayout(mLinearLayout,true);
                mButtonMandatoryNo.setEnabled(false);
                mButtonMandatoryYes.setEnabled(false);
                mMandatory = true;
                Log.v(TAG,"AEC marked as mandatory");
                break;

        }
    }

    private void startTest() {

        if (mTestThread != null && mTestThread.isAlive()) {
            Log.v(TAG,"test Thread already running.");
            return;
        }
        mTestThread = new Thread(new AudioTestRunner(TAG, TEST_AEC, mMessageHandler) {
            public void run() {
                super.run();

                StringBuilder sb = new StringBuilder(); //test results strings
                mTestAECPassed = false;
                sendMessage(AudioTestRunner.TEST_MESSAGE,
                        "Testing Recording with AEC");

                //is AEC Available?
                if (!AcousticEchoCanceler.isAvailable()) {
                    String msg;
                    if (mMandatory) {
                        msg = "Error. AEC not available";
                        sendMessage(AudioTestRunner.TEST_ENDED_ERROR, msg);
                    } else {
                        mTestAECPassed = true;
                        msg = "Warning. AEC not implemented.";
                        sendMessage(AudioTestRunner.TEST_ENDED_OK, msg);
                    }
                    recordTestResults(mMandatory, 0, 0, msg);
                    return;
                }

                 //Step 0. Prepare system
                AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
                int targetMode = AudioManager.MODE_IN_COMMUNICATION;
                int originalMode = am.getMode();
                am.setMode(targetMode);

                if (am.getMode() != targetMode) {
                    sendMessage(AudioTestRunner.TEST_ENDED_ERROR,
                            "Couldn't set mode to MODE_IN_COMMUNICATION.");
                    return;
                }

                int playbackStreamType = AudioManager.STREAM_VOICE_CALL;
                int maxLevel = getMaxLevelForStream(playbackStreamType);
                int desiredLevel = maxLevel - 1;
                setLevelForStream(playbackStreamType, desiredLevel);

                int currentLevel = getLevelForStream(playbackStreamType);
                if (currentLevel != desiredLevel) {
                    am.setMode(originalMode);
                    sendMessage(AudioTestRunner.TEST_ENDED_ERROR,
                            "Couldn't set level for STREAM_VOICE_CALL. Expected " +
                                    desiredLevel +" got: " + currentLevel);
                    return;
                }

                boolean originalSpeakerPhone = am.isSpeakerphoneOn();
                am.setSpeakerphoneOn(true);

                //Step 1. With AEC (on by Default when using VOICE_COMMUNICATION audio source).
                mSPlayer.setStreamType(playbackStreamType);
                mSPlayer.setSoundWithResId(getApplicationContext(), R.raw.speech);
                mSRecorder.startRecording();

                //get AEC
                int audioSessionId = mSRecorder.getAudioSessionId();
                if (mAec != null) {
                    mAec.release();
                    mAec = null;
                }
                try {
                    mAec = AcousticEchoCanceler.create(audioSessionId);
                } catch (Exception e) {
                    mSRecorder.stopRecording();
                    String msg = "Could not create AEC Effect. " + e.toString();
                    recordTestResults(mMandatory, 0, 0, msg);
                    am.setSpeakerphoneOn(originalSpeakerPhone);
                    am.setMode(originalMode);
                    sendMessage(AudioTestRunner.TEST_ENDED_ERROR, msg);
                    return;
                }

                if (mAec == null) {
                    mSRecorder.stopRecording();
                    String msg = "Could not create AEC Effect (AEC Null)";
                    recordTestResults(mMandatory,0, 0, msg);
                    am.setSpeakerphoneOn(originalSpeakerPhone);
                    am.setMode(originalMode);
                    sendMessage(AudioTestRunner.TEST_ENDED_ERROR, msg);
                    return;
                }

                if (!mAec.getEnabled()) {
                    String msg = "AEC is not enabled by default.";
                    if (mMandatory) {
                        mSRecorder.stopRecording();
                        recordTestResults(mMandatory,0, 0, msg);
                        am.setSpeakerphoneOn(originalSpeakerPhone);
                        am.setMode(originalMode);
                        sendMessage(AudioTestRunner.TEST_ENDED_ERROR, msg);
                        return;
                    } else {
                        sb.append("Warning. " + msg + "\n");
                    }
                }

                mRMSPlayer1.reset();
                mRMSRecorder1.reset();
                mSPlayer.play(true);
                mRMSPlayer1.setRunning(true);
                mRMSRecorder1.setRunning(true);

                for (int s = 0; s < SHOT_COUNT; s++) {
                    sleep(SHOT_FREQUENCY_MS);
                    mRMSRecorder1.captureShot();
                    mRMSPlayer1.captureShot();

                    sendMessage(AudioTestRunner.TEST_MESSAGE,
                            String.format("AEC ON. Rec: %.2f dB, Play: %.2f dB",
                                    20 * Math.log10(mRMSRecorder1.getRmsCurrent()),
                                    20 * Math.log10(mRMSPlayer1.getRmsCurrent())));
                }

                mRMSPlayer1.setRunning(false);
                mRMSRecorder1.setRunning(false);
                mSPlayer.play(false);

                int lastShot = SHOT_COUNT - 1;
                int firstShot = SHOT_COUNT - SHOT_COUNT_CORRELATION;

                double maxAEC = computeAcousticCouplingFactor(mRMSPlayer1.getRmsSnapshots(),
                        mRMSRecorder1.getRmsSnapshots(), firstShot, lastShot);
                sendMessage(AudioTestRunner.TEST_MESSAGE,
                        String.format("AEC On: Acoustic Coupling: %.2f", maxAEC));

                //Wait
                sleep(1000);
                sendMessage(AudioTestRunner.TEST_MESSAGE, "Testing Recording AEC OFF");

                //Step 2. Turn off the AEC
                mSPlayer.setSoundWithResId(getApplicationContext(),
                        R.raw.speech);
                mAec.setEnabled(false);

                // mSRecorder.startRecording();
                mRMSPlayer2.reset();
                mRMSRecorder2.reset();
                mSPlayer.play(true);
                mRMSPlayer2.setRunning(true);
                mRMSRecorder2.setRunning(true);

                for (int s = 0; s < SHOT_COUNT; s++) {
                    sleep(SHOT_FREQUENCY_MS);
                    mRMSRecorder2.captureShot();
                    mRMSPlayer2.captureShot();

                    sendMessage(AudioTestRunner.TEST_MESSAGE,
                            String.format("AEC OFF. Rec: %.2f dB, Play: %.2f dB",
                                    20 * Math.log10(mRMSRecorder2.getRmsCurrent()),
                                    20 * Math.log10(mRMSPlayer2.getRmsCurrent())));
                }

                mRMSPlayer2.setRunning(false);
                mRMSRecorder2.setRunning(false);
                mSRecorder.stopRecording();
                mSPlayer.play(false);

                am.setSpeakerphoneOn(originalSpeakerPhone);
                am.setMode(originalMode);

                double maxNoAEC = computeAcousticCouplingFactor(mRMSPlayer2.getRmsSnapshots(),
                        mRMSRecorder2.getRmsSnapshots(), firstShot, lastShot);
                sendMessage(AudioTestRunner.TEST_MESSAGE, String.format("AEC Off: Corr: %.2f",
                        maxNoAEC));

                //test decision
                boolean testPassed = true;

                sb.append(String.format(" Acoustic Coupling AEC ON: %.2f <= %.2f : ", maxAEC,
                        TEST_THRESHOLD_AEC_ON));
                if (maxAEC <= TEST_THRESHOLD_AEC_ON) {
                    sb.append("SUCCESS\n");
                } else {
                    sb.append("FAILED\n");
                    testPassed = false;
                }

                sb.append(String.format(" Acoustic Coupling AEC OFF: %.2f >= %.2f : ", maxNoAEC,
                        TEST_THRESHOLD_AEC_OFF));
                if (maxNoAEC >= TEST_THRESHOLD_AEC_OFF) {
                    sb.append("SUCCESS\n");
                } else {
                    sb.append("FAILED\n");
                    testPassed = false;
                }

                mTestAECPassed = testPassed;

                if (mTestAECPassed) {
                    sb.append("All Tests Passed");
                } else {
                    if (mMandatory) {
                        sb.append("Test failed. Please fix issues and try again");
                    } else {
                        sb.append("Warning. Acoustic Coupling Levels did not pass criteria");
                        mTestAECPassed = true;
                    }
                }

                recordTestResults(mMandatory, maxAEC, maxNoAEC, sb.toString());

                //compute results.
                sendMessage(AudioTestRunner.TEST_ENDED_OK, "\n" + sb.toString());
            }
        });
        mTestThread.start();
    }

    private void recordTestResults(boolean aecMandatory, double maxAEC, double maxNoAEC,
                                   String msg) {

        getReportLog().addValue("AEC_mandatory",
                aecMandatory,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

        getReportLog().addValue("max_with_AEC",
                maxAEC,
                ResultType.LOWER_BETTER,
                ResultUnit.SCORE);

        getReportLog().addValue("max_without_AEC",
                maxNoAEC,
                ResultType.HIGHER_BETTER,
                ResultUnit.SCORE);

        getReportLog().addValue("result_string",
                msg,
                ResultType.NEUTRAL,
                ResultUnit.NONE);

    }

    //TestMessageHandler
    private AudioTestRunner.AudioTestRunnerMessageHandler mMessageHandler =
            new AudioTestRunner.AudioTestRunnerMessageHandler() {
        @Override
        public void testStarted(int testId, String str) {
            super.testStarted(testId, str);
            Log.v(TAG, "Test Started! " + testId + " str:"+str);
            showView(mProgress, true);
            mTestAECPassed = false;
            getPassButton().setEnabled(false);
            mResultTest.setText("test in progress..");
        }

        @Override
        public void testMessage(int testId, String str) {
            super.testMessage(testId, str);
            Log.v(TAG, "Message TestId: " + testId + " str:"+str);
            mResultTest.setText("test in progress.. " + str);
        }

        @Override
        public void testEndedOk(int testId, String str) {
            super.testEndedOk(testId, str);
            Log.v(TAG, "Test EndedOk. " + testId + " str:"+str);
            showView(mProgress, false);
            mResultTest.setText("test completed. " + str);
            if (mTestAECPassed) {
                getPassButton().setEnabled(true);;
            }
        }

        @Override
        public void testEndedError(int testId, String str) {
            super.testEndedError(testId, str);
            Log.v(TAG, "Test EndedError. " + testId + " str:"+str);
            showView(mProgress, false);
            mResultTest.setText("test failed. " + str);
        }
    };
}
