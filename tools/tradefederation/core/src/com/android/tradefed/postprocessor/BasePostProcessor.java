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
package com.android.tradefed.postprocessor;

import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.ListMultimap;

import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * The base {@link IPostProcessor} that every implementation should extend. Ensure that the post
 * processing methods are called before the final result reporters.
 *
 * <p>TODO: expand to file post-processing too if needed.
 */
public abstract class BasePostProcessor implements IPostProcessor {

    @Option(name = "disable", description = "disables the post processor.")
    private boolean mDisable = false;

    private ITestInvocationListener mForwarder;
    private ArrayListMultimap<String, Metric> storedTestMetrics = ArrayListMultimap.create();
    private Map<TestDescription, Map<String, LogFile>> mTestLogs = new LinkedHashMap<>();
    private Map<String, LogFile> mRunLogs = new HashMap<>();
    // Keeps track of the current test; takes null value when the post processor is not in the scope
    // of any test (i.e. before the first test, in-between tests and after the last test).
    private TestDescription mCurrentTest = null;

    /** {@inheritDoc} */
    @Override
    public abstract Map<String, Metric.Builder> processRunMetricsAndLogs(
            HashMap<String, Metric> rawMetrics, Map<String, LogFile> runLogs);

    /** {@inheritDoc} */
    @Override
    public Map<String, Metric.Builder> processTestMetricsAndLogs(
            TestDescription testDescription,
            HashMap<String, Metric> testMetrics,
            Map<String, LogFile> testLogs) {
        return new HashMap<String, Metric.Builder>();
    }

    /** {@inheritDoc} */
    @Override
    public Map<String, Metric.Builder> processAllTestMetricsAndLogs(
            ListMultimap<String, Metric> allTestMetrics,
            Map<TestDescription, Map<String, LogFile>> allTestLogs) {
        return new HashMap<String, Metric.Builder>();
    }

    /** =================================== */
    /** {@inheritDoc} */
    @Override
    public final ITestInvocationListener init(ITestInvocationListener listener) {
        mForwarder = listener;
        return this;
    }

    /** =================================== */
    /** {@inheritDoc} */
    @Override
    public final boolean isDisabled() {
        return mDisable;
    }

    /** =================================== */
    /** Invocation Listeners for forwarding */
    @Override
    public final void invocationStarted(IInvocationContext context) {
        mForwarder.invocationStarted(context);
    }

    @Override
    public final void invocationFailed(Throwable cause) {
        mForwarder.invocationFailed(cause);
    }

    @Override
    public final void invocationFailed(FailureDescription failure) {
        mForwarder.invocationFailed(failure);
    }

    @Override
    public final void invocationEnded(long elapsedTime) {
        mForwarder.invocationEnded(elapsedTime);
    }

    @Override
    public final void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        mForwarder.testLog(dataName, dataType, dataStream);
    }

    @Override
    public final void testModuleStarted(IInvocationContext moduleContext) {
        mForwarder.testModuleStarted(moduleContext);
    }

    @Override
    public final void testModuleEnded() {
        mForwarder.testModuleEnded();
    }

    /** Test run callbacks */
    @Override
    public final void testRunStarted(String runName, int testCount) {
        mForwarder.testRunStarted(runName, testCount);
    }

    @Override
    public final void testRunStarted(String runName, int testCount, int attemptNumber) {
        mForwarder.testRunStarted(runName, testCount, attemptNumber);
    }

    @Override
    public final void testRunFailed(String errorMessage) {
        mForwarder.testRunFailed(errorMessage);
    }

    @Override
    public final void testRunFailed(FailureDescription failure) {
        mForwarder.testRunFailed(failure);
    }

    @Override
    public final void testRunStopped(long elapsedTime) {
        mForwarder.testRunStopped(elapsedTime);
    }

    @Override
    public final void testRunEnded(long elapsedTime, Map<String, String> runMetrics) {
        testRunEnded(elapsedTime, TfMetricProtoUtil.upgradeConvert(runMetrics));
    }

    @Override
    public final void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        try {
            HashMap<String, Metric> rawValues = getRawMetricsOnly(runMetrics);
            // Add post-processed run metrics.
            Map<String, Metric.Builder> postprocessedResults =
                    processRunMetricsAndLogs(rawValues, mRunLogs);
            addProcessedMetricsToExistingMetrics(postprocessedResults, runMetrics);
            // Add aggregated test metrics (results from post-processing all test metrics).
            Map<String, Metric.Builder> aggregateResults =
                    processAllTestMetricsAndLogs(storedTestMetrics, mTestLogs);
            addProcessedMetricsToExistingMetrics(aggregateResults, runMetrics);
        } catch (RuntimeException e) {
            // Prevent exception from messing up the status reporting.
            CLog.e(e);
        } finally {
            // Clear out the stored test metrics.
            storedTestMetrics.clear();
            // Clear out the stored test and run logs.
            mTestLogs.clear();
            mRunLogs.clear();
        }
        mForwarder.testRunEnded(elapsedTime, runMetrics);
    }

    /** Test cases callbacks */
    @Override
    public final void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    @Override
    public final void testStarted(TestDescription test, long startTime) {
        mCurrentTest = test;
        if (mTestLogs.containsKey(test)) {
            mTestLogs.get(test).clear();
        } else {
            mTestLogs.put(test, new HashMap<String, LogFile>());
        }

        mForwarder.testStarted(test, startTime);
    }

    @Override
    public final void testFailed(TestDescription test, String trace) {
        mForwarder.testFailed(test, trace);
    }

    @Override
    public final void testFailed(TestDescription test, FailureDescription failure) {
        mForwarder.testFailed(test, failure);
    }

    @Override
    public final void testEnded(TestDescription test, Map<String, String> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, Map<String, String> testMetrics) {
        testEnded(test, endTime, TfMetricProtoUtil.upgradeConvert(testMetrics));
    }

    @Override
    public final void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        mCurrentTest = null;
        try {
            HashMap<String, Metric> rawValues = getRawMetricsOnly(testMetrics);
            // Store the raw metrics from the test in storedTestMetrics for potential aggregation.
            for (Map.Entry<String, Metric> entry : rawValues.entrySet()) {
                storedTestMetrics.put(entry.getKey(), entry.getValue());
            }
            Map<String, Metric.Builder> results =
                    processTestMetricsAndLogs(
                            test,
                            rawValues,
                            mTestLogs.containsKey(test)
                                    ? mTestLogs.get(test)
                                    : new HashMap<String, LogFile>());
            for (Entry<String, Metric.Builder> newEntry : results.entrySet()) {
                String newKey = newEntry.getKey();
                if (testMetrics.containsKey(newKey)) {
                    CLog.e(
                            "Key '%s' is already asssociated with a metric and will not be "
                                    + "replaced.",
                            newKey);
                    continue;
                }
                // Force the metric to 'processed' since generated in a post-processor.
                Metric newMetric =
                        newEntry.getValue()
                                .setType(getMetricType())
                                .build();
                testMetrics.put(newKey, newMetric);
            }
        } catch (RuntimeException e) {
            // Prevent exception from messing up the status reporting.
            CLog.e(e);
        }
        mForwarder.testEnded(test, endTime, testMetrics);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, String trace) {
        mForwarder.testAssumptionFailure(test, trace);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, FailureDescription failure) {
        mForwarder.testAssumptionFailure(test, failure);
    }

    @Override
    public final void testIgnored(TestDescription test) {
        mForwarder.testIgnored(test);
    }

    @Override
    public final void setLogSaver(ILogSaver logSaver) {
        if (mForwarder instanceof ILogSaverListener) {
            ((ILogSaverListener) mForwarder).setLogSaver(logSaver);
        }
    }

    @Override
    public final void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        if (mForwarder instanceof ILogSaverListener) {
            ((ILogSaverListener) mForwarder).testLogSaved(dataName, dataType, dataStream, logFile);
        }
    }

    /**
     * {@inheritDoc}
     *
     * <p>Updates the log-to-test assocation. If this method is called during a test, then the log
     * belongs to the test; otherwise it will be a run log.
     */
    @Override
    public final void logAssociation(String dataName, LogFile logFile) {
        // mCurrentTest is null only outside the scope of a test.
        if (mCurrentTest != null) {
            mTestLogs.get(mCurrentTest).put(dataName, logFile);
        } else {
            mRunLogs.put(dataName, logFile);
        }

        if (mForwarder instanceof ILogSaverListener) {
            ((ILogSaverListener) mForwarder).logAssociation(dataName, logFile);
        }
    }

    // Internal utilities

    /**
     * We only allow post-processing of raw values. Already processed values will not be considered.
     */
    private HashMap<String, Metric> getRawMetricsOnly(HashMap<String, Metric> runMetrics) {
        HashMap<String, Metric> rawMetrics = new HashMap<>();
        for (Entry<String, Metric> entry : runMetrics.entrySet()) {
            if (DataType.RAW.equals(entry.getValue().getType())) {
                rawMetrics.put(entry.getKey(), entry.getValue());
            }
        }
        return rawMetrics;
    }

    /** Add processed metrics to the metrics to be reported. */
    private void addProcessedMetricsToExistingMetrics(
            Map<String, Metric.Builder> processed, Map<String, Metric> existing) {
        for (Entry<String, Metric.Builder> newEntry : processed.entrySet()) {
            String newKey = newEntry.getKey();
            if (existing.containsKey(newKey)) {
                CLog.e(
                        "Key '%s' is already asssociated with a metric and will not be "
                                + "replaced.",
                        newKey);
                continue;
            }
            // Force the metric to 'processed' since generated in a post-processor.
            Metric newMetric =
                    newEntry.getValue()
                            .setType(getMetricType())
                            .build();
            existing.put(newKey, newMetric);
        }
    }

    /**
     * Override this method to change the metric type if needed. By default metric is set to
     * processed type.
     */
    protected DataType getMetricType() {
        return DataType.PROCESSED;
    }
}
