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

package com.android.media.tests;

import com.android.tradefed.log.LogUtil;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

public class CameraTestMetricsCollectionListener {

protected static abstract class AbstractCameraTestMetricsCollectionListener extends CollectingTestListener {
    private ITestInvocationListener mListener;
    private Map<String, String> mMetrics = new HashMap<>();
    private Map<String, String> mFatalErrors = new HashMap<>();
    private CameraTestBase mCameraTestBase;

    private static final String INCOMPLETE_TEST_ERR_MSG_PREFIX =
            "Test failed to run to completion. Reason: 'Instrumentation run failed";

    public AbstractCameraTestMetricsCollectionListener(ITestInvocationListener listener) {
        mListener = listener;
        mCameraTestBase = new CameraTestBase();
    }
    /**
     * Report the end of an individual camera test and delegate handling the collected metrics to
     * subclasses. Do not override testEnded to manipulate the test metrics after each test.
     * Instead, use handleMetricsOnTestEnded.
     *
     * @param test identifies the test
     * @param testMetrics a {@link Map} of the metrics emitted
     */
    @Override
    public void testEnded(
            TestDescription test,
            long endTime,
            HashMap<String, MetricMeasurement.Metric> testMetrics) {
        super.testEnded(test, endTime, testMetrics);
        handleMetricsOnTestEnded(test, TfMetricProtoUtil.compatibleConvert(testMetrics));
        stopDumping(test);
        mListener.testEnded(test, endTime, testMetrics);
    }

    @Override
    public void testStarted(TestDescription test, long startTime) {
        super.testStarted(test, startTime);
        startDumping(test);
        mListener.testStarted(test, startTime);
    }

    @Override
    public void testFailed(TestDescription test, String trace) {
        super.testFailed(test, trace);
        // If the test failed to run to complete, this is an exceptional case.
        // Let this test run fail so that it can rerun.
        if (trace.startsWith(INCOMPLETE_TEST_ERR_MSG_PREFIX)) {
            mFatalErrors.put(test.getTestName(), trace);
            LogUtil.CLog.d("Test (%s) failed due to fatal error : %s", test.getTestName(), trace);
        }
        mListener.testFailed(test, trace);
    }

    @Override
    public void testRunFailed(String errorMessage) {
        super.testRunFailed(errorMessage);
        mFatalErrors.put(mCameraTestBase.getRuKey(), errorMessage);
    }

    @Override
    public void testRunEnded(
            long elapsedTime, HashMap<String, MetricMeasurement.Metric> runMetrics) {
        super.testRunEnded(elapsedTime, runMetrics);
        handleTestRunEnded(mListener, elapsedTime, TfMetricProtoUtil.compatibleConvert(runMetrics));
        // never be called since handleTestRunEnded will handle it if needed.
        // mListener.testRunEnded(elapsedTime, runMetrics);
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        super.testRunStarted(runName, testCount);
        mListener.testRunStarted(runName, testCount);
    }

    @Override
    public void testRunStopped(long elapsedTime) {
        super.testRunStopped(elapsedTime);
        mListener.testRunStopped(elapsedTime);
    }

    @Override
    public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        super.testLog(dataName, dataType, dataStream);
        mListener.testLog(dataName, dataType, dataStream);
    }

    protected void startDumping(TestDescription test) {
        if (mCameraTestBase.shouldDumpMeminfo()) {
            mCameraTestBase.mMeminfoTimer.start(test);
        }
        if (mCameraTestBase.shouldDumpThreadCount()) {
            mCameraTestBase.mThreadTrackerTimer.start(test);
        }
    }

    protected void stopDumping(TestDescription test) {
        InputStreamSource outputSource = null;
        File outputFile = null;
        if (mCameraTestBase.shouldDumpMeminfo()) {
            mCameraTestBase.mMeminfoTimer.stop();
            // Grab a snapshot of meminfo file and post it to dashboard.
            try {
                outputFile = mCameraTestBase.mMeminfoTimer.getOutputFile();
                outputSource = new FileInputStreamSource(outputFile, true /* delete */);
                String logName = String.format("meminfo_%s", test.getTestName());
                mListener.testLog(logName, LogDataType.TEXT, outputSource);
            } finally {
                StreamUtil.cancel(outputSource);
            }
        }
        if (mCameraTestBase.shouldDumpThreadCount()) {
            mCameraTestBase.mThreadTrackerTimer.stop();
            try {
                outputFile = mCameraTestBase.mThreadTrackerTimer.getOutputFile();
                outputSource = new FileInputStreamSource(outputFile, true /* delete */);
                String logName = String.format("ps_%s", test.getTestName());
                mListener.testLog(logName, LogDataType.TEXT, outputSource);
            } finally {
                StreamUtil.cancel(outputSource);
            }
        }
    }

    public Map<String, String> getAggregatedMetrics() {
        return mMetrics;
    }

    public ITestInvocationListener getListeners() {
        return mListener;
    }

    /**
     * Determine that the test run failed with fatal errors.
     *
     * @return True if test run has a failure due to fatal error.
     */
    public boolean hasTestRunFatalError() {
        return (getNumTotalTests() > 0 && mFatalErrors.size() > 0);
    }

    public Map<String, String> getFatalErrors() {
        return mFatalErrors;
    }

    public String getErrorMessage() {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> error : mFatalErrors.entrySet()) {
            sb.append(error.getKey());
            sb.append(" : ");
            sb.append(error.getValue());
            sb.append("\n");
        }
        return sb.toString();
    }

    abstract void handleMetricsOnTestEnded(TestDescription test, Map<String, String> testMetrics);

    abstract void handleTestRunEnded(
            ITestInvocationListener listener, long elapsedTime, Map<String, String> runMetrics);
}

protected static class DefaultCollectingListener extends AbstractCameraTestMetricsCollectionListener {
    private CameraTestBase mCameraTestBase;

    public DefaultCollectingListener(ITestInvocationListener listener) {
        super(listener);
        mCameraTestBase = new CameraTestBase();
    }

    @Override
    public void handleMetricsOnTestEnded(TestDescription test, Map<String, String> testMetrics) {
        if (testMetrics == null) {
            return; // No-op if there is nothing to post.
        }
        getAggregatedMetrics().putAll(testMetrics);
    }

    @Override
    public void handleTestRunEnded(
            ITestInvocationListener listener, long elapsedTime, Map<String, String> runMetrics) {
        // Post aggregated metrics at the end of test run.
        listener.testRunEnded(
                mCameraTestBase.getTestDurationMs(),
                TfMetricProtoUtil.upgradeConvert(getAggregatedMetrics()));
    }
 }
}
