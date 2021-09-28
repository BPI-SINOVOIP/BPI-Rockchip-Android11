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
package com.android.tradefed.result;

import com.android.loganalysis.item.JavaCrashItem;
import com.android.loganalysis.item.LogcatItem;
import com.android.loganalysis.parser.LogcatParser;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.util.StreamUtil;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Special listener: on failures (instrumentation process crashing) it will attempt to extract from
 * the logcat the crash and adds it to the failure message associated with the test.
 */
public class LogcatCrashResultForwarder extends ResultForwarder {

    /** Special error message from the instrumentation when something goes wrong on device side. */
    public static final String ERROR_MESSAGE = "Process crashed.";
    public static final String SYSTEM_CRASH_MESSAGE = "System has crashed.";

    public static final int MAX_NUMBER_CRASH = 3;

    private Long mStartTime = null;
    private Long mLastStartTime = null;
    private ITestDevice mDevice;
    private LogcatItem mLogcatItem = null;

    public LogcatCrashResultForwarder(ITestDevice device, ITestInvocationListener... listeners) {
        super(listeners);
        mDevice = device;
    }

    public ITestDevice getDevice() {
        return mDevice;
    }

    @Override
    public void testStarted(TestDescription test, long startTime) {
        mStartTime = startTime;
        super.testStarted(test, startTime);
    }

    @Override
    public void testFailed(TestDescription test, String trace) {
        // If the test case was detected as crashing the instrumentation, we add the crash to it.
        trace = extractCrashAndAddToMessage(trace, mStartTime);
        super.testFailed(test, trace);
    }

    @Override
    public void testFailed(TestDescription test, FailureDescription failure) {
        // If the test case was detected as crashing the instrumentation, we add the crash to it.
        String trace = extractCrashAndAddToMessage(failure.getErrorMessage(), mStartTime);
        failure.setErrorMessage(trace);
        super.testFailed(test, failure);
    }

    @Override
    public void testEnded(TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        super.testEnded(test, endTime, testMetrics);
        mLastStartTime = mStartTime;
        mStartTime = null;
    }

    @Override
    public void testRunFailed(String errorMessage) {
        testRunFailed(FailureDescription.create(errorMessage, FailureStatus.TEST_FAILURE));
    }

    @Override
    public void testRunFailed(FailureDescription error) {
        // Also add the failure to the run failure if the testFailed generated it.
        // A Process crash would end the instrumentation, so a testRunFailed is probably going to
        // be raised for the same reason.
        String errorMessage = error.getErrorMessage();
        if (mLogcatItem != null) {
            errorMessage = addJavaCrashToString(mLogcatItem, errorMessage);
            mLogcatItem = null;
        } else {
            errorMessage = extractCrashAndAddToMessage(errorMessage, mLastStartTime);
        }
        error.setErrorMessage(errorMessage);
        if (isCrash(errorMessage)) {
            error.setErrorIdentifier(DeviceErrorIdentifier.INSTRUMENATION_CRASH);
        }
        super.testRunFailed(error);
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        super.testRunEnded(elapsedTime, runMetrics);
        mLastStartTime = null;
    }

    /** Attempt to extract the crash from the logcat if the test was seen as started. */
    private String extractCrashAndAddToMessage(String errorMessage, Long startTime) {
        if (isCrash(errorMessage) && startTime != null) {
            mLogcatItem = extractLogcat(mDevice, startTime);
            errorMessage = addJavaCrashToString(mLogcatItem, errorMessage);
        }
        return errorMessage;
    }

    private boolean isCrash(String errorMessage) {
        return errorMessage.contains(ERROR_MESSAGE) || errorMessage.contains(SYSTEM_CRASH_MESSAGE);
    }

    /**
     * Extract a formatted object from the logcat snippet.
     *
     * @param device The device from which to pull the logcat.
     * @param startTime The beginning time of the last tests.
     * @return A {@link LogcatItem} that contains the information inside the logcat.
     */
    private LogcatItem extractLogcat(ITestDevice device, long startTime) {
        try (InputStreamSource logSource = device.getLogcatSince(startTime)) {
            if (logSource.size() == 0L) {
                return null;
            }
            String message = StreamUtil.getStringFromStream(logSource.createInputStream());
            LogcatParser parser = new LogcatParser();
            List<String> lines = Arrays.asList(message.split("\n"));
            return parser.parse(lines);
        } catch (IOException e) {
            CLog.e(e);
        }
        return null;
    }

    /** Append the Java crash information to the failure message. */
    private String addJavaCrashToString(LogcatItem item, String errorMsg) {
        if (item == null) {
            return errorMsg;
        }
        List<String> crashes = dedupCrash(item.getJavaCrashes());
        int displayed = Math.min(crashes.size(), MAX_NUMBER_CRASH);
        for (int i = 0; i < displayed; i++) {
            errorMsg = String.format("%s\nCrash Message:%s\n", errorMsg, crashes.get(i));
        }
        return errorMsg;
    }

    /** Remove identical crash from the list of errors. */
    private List<String> dedupCrash(List<JavaCrashItem> origList) {
        LinkedHashSet<String> dedupList = new LinkedHashSet<>();
        for (JavaCrashItem item : origList) {
            dedupList.add(String.format("%s\n%s", item.getMessage(), item.getStack()));
        }
        return new ArrayList<>(dedupList);
    }
}
