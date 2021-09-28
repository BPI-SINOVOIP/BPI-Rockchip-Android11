/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed.testtype.suite;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceProperties;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import com.google.common.annotations.VisibleForTesting;

import java.util.List;

/**
 * Listener used to take action such as screenshot, bugreport, logcat collection upon a test failure
 * when requested.
 */
public class TestFailureListener implements ITestInvocationListener {

    private List<ITestDevice> mListDevice;
    private ITestLogger mLogger;
    // Settings for the whole invocation
    private boolean mBugReportOnFailure;
    private boolean mRebootOnFailure;

    // module specific values
    private boolean mModuleBugReportOnFailure = true;

    public TestFailureListener(
            List<ITestDevice> devices, boolean bugReportOnFailure, boolean rebootOnFailure) {
        mListDevice = devices;
        mBugReportOnFailure = bugReportOnFailure;
        mRebootOnFailure = rebootOnFailure;
    }

    /** {@inheritDoc} */
    @Override
    public void testFailed(TestDescription test, String trace) {
        CLog.i("FailureListener.testFailed %s %b", test.toString(), mBugReportOnFailure);
        for (ITestDevice device : mListDevice) {
            captureFailure(device, test);
        }
    }

    /** Capture the appropriate logs for one device for one test failure. */
    private void captureFailure(ITestDevice device, TestDescription test) {
        String serial = device.getSerialNumber();
        if (mBugReportOnFailure && mModuleBugReportOnFailure) {
            if (!device.logBugreport(
                    String.format("%s-%s-bugreport", test.toString(), serial), mLogger)) {
                CLog.e("Failed to capture bugreport for %s failure on %s.", test, serial);
            }
        }
        if (mRebootOnFailure) {
            try {
                // Rebooting on all failures can hide legitimate issues and platform instabilities,
                // therefore only allowed on "user-debug" and "eng" builds.
                if ("user".equals(device.getProperty(DeviceProperties.BUILD_TYPE))) {
                    CLog.e("Reboot-on-failure should only be used during development," +
                            " this is a\" user\" build device");
                } else {
                    device.reboot();
                }
            } catch (DeviceNotAvailableException e) {
                CLog.e(e);
                CLog.e("Device %s became unavailable while rebooting", serial);
            }
        }
    }

    /** Join on all the logcat capturing threads to ensure they terminate. */
    public void join() {
        // Reset the module config to use the invocation settings by default.
        mModuleBugReportOnFailure = true;
    }

    /**
     * Forward the log to the logger, do not do it from whitin the #testLog callback as if
     * TestFailureListener is part of the chain, it will results in an infinite loop.
     */
    public void testLogForward(
            String dataName, LogDataType dataType, InputStreamSource dataStream) {
        mLogger.testLog(dataName, dataType, dataStream);
    }

    @Override
    public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        // Explicitly do nothing on testLog
    }

    /**
     * Get the default {@link IRunUtil} instance
     */
    @VisibleForTesting
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    /**
     * Allows to override the invocation settings of capture on failure by the module specific
     * configurations.
     *
     * @param bugreportOnFailure true to capture a bugreport on test failure. False otherwise.
     */
    public void applyModuleConfiguration(boolean bugreportOnFailure) {
        mModuleBugReportOnFailure = bugreportOnFailure;
    }

    /** Sets where the logs should be saved. */
    public void setLogger(ITestLogger logger) {
        mLogger = logger;
    }
}
