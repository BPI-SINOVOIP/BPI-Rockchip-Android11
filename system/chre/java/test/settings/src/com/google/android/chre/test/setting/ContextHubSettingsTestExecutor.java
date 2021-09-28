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
package com.google.android.chre.test.setting;

import android.content.Context;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppBinary;
import android.hardware.location.NanoAppMessage;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreSettingsTest;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A class that can execute the CHRE settings test.
 */
public class ContextHubSettingsTestExecutor extends ContextHubClientCallback {
    private static final String TAG = "ContextHubSettingsTestExecutor";

    private static final long TEST_TIMEOUT_SECONDS = 30;

    private final NanoAppBinary mNanoAppBinary;

    private final long mNanoAppId;

    private final ContextHubClient mContextHubClient;

    private final AtomicReference<ChreSettingsTest.TestResult> mTestResult =
                new AtomicReference<>();

    private final AtomicBoolean mChreReset = new AtomicBoolean(false);

    private final ContextHubManager mContextHubManager;

    private final ContextHubInfo mContextHubInfo;

    private CountDownLatch mCountDownLatch;

    private boolean mInitialized = false;

    private final AtomicBoolean mTestSetupComplete = new AtomicBoolean(false);

    public ContextHubSettingsTestExecutor(NanoAppBinary binary) {
        mNanoAppBinary = binary;
        mNanoAppId = mNanoAppBinary.getNanoAppId();
        Context context = InstrumentationRegistry.getTargetContext();
        mContextHubManager =
            (ContextHubManager) context.getSystemService(Context.CONTEXTHUB_SERVICE);
        Assert.assertTrue(mContextHubManager != null);

        List<ContextHubInfo> info = mContextHubManager.getContextHubs();
        Assert.assertTrue(info.size() > 0);
        mContextHubInfo = info.get(0);

        mContextHubClient = mContextHubManager.createClient(mContextHubInfo, this);
        Assert.assertTrue(mContextHubClient != null);
    }

    @Override
    public void onMessageFromNanoApp(ContextHubClient client, NanoAppMessage message) {
        if (message.getNanoAppId() == mNanoAppId) {
            boolean valid = true;
            switch (message.getMessageType()) {
                case ChreSettingsTest.MessageType.TEST_RESULT_VALUE: {
                    try {
                        mTestResult.set(
                                ChreSettingsTest.TestResult.parseFrom(message.getMessageBody()));
                        Log.d(TAG, "Got test result message with code: "
                                + mTestResult.get().getCode());
                    } catch (InvalidProtocolBufferException e) {
                        Log.e(TAG, "Failed to parse message: " + e.getMessage());
                    }
                    break;
                }
                case ChreSettingsTest.MessageType.TEST_SETUP_COMPLETE_VALUE: {
                    mTestSetupComplete.set(true);
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
        mChreReset.set(true);
        if (mCountDownLatch != null) {
            mCountDownLatch.countDown();
        }
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
     * Sets up a test for a given feature, and waits for a completed message from the nanoapp.
     *
     * @param feature The feature to set the test up for.
     */
    public void setupTestAssertSuccess(ChreSettingsTest.TestCommand.Feature feature) {
        mTestResult.set(null);
        mTestSetupComplete.set(false);
        mCountDownLatch = new CountDownLatch(1);

        ChreSettingsTest.TestCommand testCommand =
                ChreSettingsTest.TestCommand.newBuilder()
                        .setFeature(feature)
                        .setStep(ChreSettingsTest.TestCommand.Step.SETUP).build();
        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreSettingsTest.MessageType.TEST_COMMAND_VALUE,
                testCommand.toByteArray());
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Failed to send message: result = " + result);
        }

        try {
            mCountDownLatch.await(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }

        Assert.assertTrue(
                "Failed to set up test", mTestSetupComplete.get() || mTestResult.get() != null);
    }

    /**
     * Starts the CHRE settings test, and waits for a success message from the nanoapp.
     *
     * @param feature The feature to test.
     * @param state The state that the feature is at.
     */
    public void startTestAssertSuccess(
            ChreSettingsTest.TestCommand.Feature feature,
            ChreSettingsTest.TestCommand.State state) {
        mTestResult.set(null);
        mCountDownLatch = new CountDownLatch(1);

        ChreSettingsTest.TestCommand testCommand = ChreSettingsTest.TestCommand.newBuilder()
                .setFeature(feature).setState(state).build();

        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreSettingsTest.MessageType.TEST_COMMAND_VALUE,
                testCommand.toByteArray());
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Failed to send message: result = " + result);
        }

        try {
            mCountDownLatch.await(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }

        if (mTestResult.get() == null) {
            Assert.fail("No test result received");
        } else {
            Assert.assertEquals(
                    ChreSettingsTest.TestResult.Code.PASSED, mTestResult.get().getCode());
        }
    }

    /**
     * Cleans up the test, should be invoked in e.g. @After method.
     */
    public void deinit() {
        Assert.assertTrue("deinit() must be invoked after init()", mInitialized);

        if (mChreReset.get()) {
            Assert.fail("CHRE reset during the test");
        }

        ChreTestUtil.unloadNanoAppAssertSuccess(mContextHubManager, mContextHubInfo, mNanoAppId);
        mContextHubClient.close();

        mInitialized = false;
    }
}
