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
package com.google.android.chre.test.audioconcurrency;

import android.app.UiAutomation;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppBinary;
import android.hardware.location.NanoAppMessage;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreAudioConcurrencyTest;
import com.google.android.chre.nanoapp.proto.ChreTestCommon;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A class that can execute the CHRE audio concurrency test.
 */
public class ContextHubAudioConcurrencyTestExecutor extends ContextHubClientCallback {
    private static final String TAG = "ContextHubAudioConcurrencyTestExecutor";

    // Slightly longer than the timeout in the nanoapp
    private static final long TEST_TIMEOUT_SECONDS = 15;

    private static final String AUDIO_PERMISSION = "android.permission.RECORD_AUDIO";

    private final NanoAppBinary mNanoAppBinary;

    private final long mNanoAppId;

    private final ContextHubClient mContextHubClient;

    private final ContextHubManager mContextHubManager;

    private final ContextHubInfo mContextHubInfo;

    private CountDownLatch mCountDownLatch;

    private boolean mInitialized = false;

    private final AtomicBoolean mChreAudioEnabled = new AtomicBoolean(false);

    private final AtomicReference<ChreTestCommon.TestResult> mTestResult = new AtomicReference<>();

    public ContextHubAudioConcurrencyTestExecutor(
            ContextHubManager manager, ContextHubInfo info, NanoAppBinary binary) {
        mContextHubManager = manager;
        mContextHubInfo = info;
        mNanoAppBinary = binary;
        mNanoAppId = mNanoAppBinary.getNanoAppId();

        mContextHubClient = mContextHubManager.createClient(mContextHubInfo, this);
        Assert.assertTrue(mContextHubClient != null);
    }

    @Override
    public void onMessageFromNanoApp(ContextHubClient client, NanoAppMessage message) {
        if (message.getNanoAppId() == mNanoAppId) {
            boolean valid = true;
            switch (message.getMessageType()) {
                case ChreAudioConcurrencyTest.MessageType.TEST_RESULT_VALUE: {
                    try {
                        mTestResult.set(
                                ChreTestCommon.TestResult.parseFrom(message.getMessageBody()));
                        Log.d(TAG, "Got test result message with code: "
                                + mTestResult.get().getCode());
                    } catch (InvalidProtocolBufferException e) {
                        Log.e(TAG, "Failed to parse message: " + e.getMessage());
                    }
                    break;
                }
                case ChreAudioConcurrencyTest.MessageType.TEST_AUDIO_ENABLED_VALUE: {
                    Log.d(TAG, "CHRE audio enabled");
                    mChreAudioEnabled.set(true);
                    break;
                }
                default: {
                    valid = false;
                    Log.e(TAG, "Unknown message type " + message.getMessageType());
                }
            }

            if (valid && mCountDownLatch != null) {
                mCountDownLatch.countDown();
            }
        }
    }

    @Override
    public void onHubReset(ContextHubClient client) {
        // TODO: Handle Reset
    }

    /**
     * Should be invoked before run() is invoked to set up the test, e.g. in a @Before method.
     */
    public void init() {
        Assert.assertFalse("init() must not be invoked when already initialized", mInitialized);
        ChreTestUtil.loadNanoAppAssertSuccess(mContextHubManager, mContextHubInfo, mNanoAppBinary);

        mInitialized = true;
    }

    /**
     * Runs the test.
     */
    public void run() {
        // Send a message to the nanoapp to enable CHRE audio
        mCountDownLatch = new CountDownLatch(1);
        sendTestCommandMessage(ChreAudioConcurrencyTest.TestCommand.Step.ENABLE_AUDIO);
        try {
            mCountDownLatch.await(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
        Assert.assertTrue("Failed to enable CHRE audio",
                mChreAudioEnabled.get() || mTestResult.get() != null);

        recordAudio();

        // Send a message to the nanoapp to verify that CHRE audio resumes
        mCountDownLatch = new CountDownLatch(1);
        sendTestCommandMessage(ChreAudioConcurrencyTest.TestCommand.Step.VERIFY_AUDIO_RESUME);
        try {
            mCountDownLatch.await(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }

        if (mTestResult.get() == null) {
            Assert.fail("No test result received");
        } else {
            Assert.assertEquals(
                    ChreTestCommon.TestResult.Code.PASSED, mTestResult.get().getCode());
        }
    }

    /**
     * Cleans up the test, should be invoked in e.g. @After method.
     */
    public void deinit() {
        Assert.assertTrue("deinit() must be invoked after init()", mInitialized);

        ChreTestUtil.unloadNanoAppAssertSuccess(mContextHubManager, mContextHubInfo, mNanoAppId);
        mContextHubClient.close();

        mInitialized = false;
    }

    /**
     * Records audio from the mic.
     */
    private void recordAudio() {
        UiAutomation automation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        Assert.assertTrue("Failed to get UI automation", automation != null);
        automation.adoptShellPermissionIdentity(AUDIO_PERMISSION);

        // Hold the mic for 1 second
        int samplingRateHz = 16000; // 16 KHz
        int durationSeconds = 1;
        int minBufferSize = AudioRecord.getMinBufferSize(
                samplingRateHz, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        int size = Math.max(minBufferSize, samplingRateHz * durationSeconds);
        AudioRecord record = new AudioRecord(
                MediaRecorder.AudioSource.MIC, samplingRateHz,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, size);
        Assert.assertTrue("AudioRecord could not be initialized.",
                record.getState() == AudioRecord.STATE_INITIALIZED);
        record.startRecording();

        byte[] buf = new byte[size];
        record.read(buf, 0 /* offsetInBytes */, size);
        Log.d(TAG, "AP read audio for " + 1000 * size / samplingRateHz + " ms");
        record.release();

        automation.dropShellPermissionIdentity();
    }

    /**
     * @param step The test step to start.
     */
    private void sendTestCommandMessage(ChreAudioConcurrencyTest.TestCommand.Step step) {
        ChreAudioConcurrencyTest.TestCommand testCommand =
                ChreAudioConcurrencyTest.TestCommand.newBuilder().setStep(step).build();
        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreAudioConcurrencyTest.MessageType.TEST_COMMAND_VALUE,
                testCommand.toByteArray());
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Failed to send message: result = " + result);
        }
    }
}
