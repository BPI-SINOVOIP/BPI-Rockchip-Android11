/*
 * Copyright (C) 2011 The Android Open Source Project
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
package com.android.tradefed.invoker;

import com.android.ddmlib.Log.LogLevel;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.retry.ISupportGranularResults;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.TimeUtil;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/**
 * A {@link ITestInvocationListener} that collects results from a invocation shard (aka an
 * invocation split to run on multiple resources in parallel), and forwards them to another
 * listener.
 */
public class ShardListener extends CollectingTestListener implements ISupportGranularResults {

    private ITestInvocationListener mMainListener;
    private IInvocationContext mModuleContext = null;
    private int mAttemptInProgress = 0;
    private boolean mEnableGranularResults = false;

    /**
     * Create a {@link ShardListener}.
     *
     * @param main the {@link ITestInvocationListener} the results should be forwarded. To prevent
     *     collisions with other {@link ShardListener}s, this object will synchronize on
     *     <var>main</var> when forwarding results. And results will only be sent once the
     *     invocation shard completes.
     */
    public ShardListener(ITestInvocationListener main) {
        mMainListener = main;
    }

    /** {@inheritDoc} */
    @Override
    public boolean supportGranularResults() {
        return mEnableGranularResults;
    }

    public void setSupportGranularResults(boolean enableGranularResults) {
        mEnableGranularResults = enableGranularResults;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void invocationStarted(IInvocationContext context) {
        super.invocationStarted(context);
        synchronized (mMainListener) {
            mMainListener.invocationStarted(context);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void invocationFailed(Throwable cause) {
        super.invocationFailed(cause);
        synchronized (mMainListener) {
            mMainListener.invocationFailed(cause);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void invocationFailed(FailureDescription failure) {
        super.invocationFailed(failure);
        synchronized (mMainListener) {
            mMainListener.invocationFailed(failure);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        // forward testLog results immediately, since result reporters might take action on it.
        synchronized (mMainListener) {
            if (mMainListener instanceof ShardMainResultForwarder) {
                // If the listener is a log saver, we should simply forward the testLog not save
                // again.
                ((ShardMainResultForwarder) mMainListener)
                        .testLogForward(dataName, dataType, dataStream);
            } else {
                mMainListener.testLog(dataName, dataType, dataStream);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        super.testLogSaved(dataName, dataType, dataStream, logFile);
        // Forward the testLogSaved callback.
        synchronized (mMainListener) {
            if (mMainListener instanceof ILogSaverListener) {
                ((ILogSaverListener) mMainListener)
                        .testLogSaved(dataName, dataType, dataStream, logFile);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void testModuleStarted(IInvocationContext moduleContext) {
        super.testModuleStarted(moduleContext);
        mModuleContext = moduleContext;
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String name, int numTests, int attemptNumber, long startTime) {
        super.testRunStarted(name, numTests, attemptNumber, startTime);
        mAttemptInProgress = attemptNumber;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void testRunFailed(String failureMessage) {
        super.testRunFailed(failureMessage);
        CLog.logAndDisplay(LogLevel.ERROR, "FAILED: %s failed with message: %s",
                getCurrentRunResults().getName(), failureMessage);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(FailureDescription failure) {
        super.testRunFailed(failure);
        CLog.logAndDisplay(
                LogLevel.ERROR,
                "FAILED: %s failed with message: %s",
                getCurrentRunResults().getName(),
                failure.toString());
    }

    /** {@inheritDoc} */
    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        super.testRunEnded(elapsedTime, runMetrics);
        CLog.logAndDisplay(
                LogLevel.INFO, "Sharded test completed: %s", getCurrentRunResults().getName());
        if (mModuleContext == null) {
            // testRunEnded only forwards if it's not part of a module. If it's a module
            // testModuleEnded is in charge of forwarding all run results.
            synchronized (mMainListener) {
                forwardRunResults(getCurrentRunResults(), mAttemptInProgress);
            }
            mAttemptInProgress = 0;
        }

    }

    /** {@inheritDoc} */
    @Override
    public void testModuleEnded() {
        super.testModuleEnded();

        synchronized (mMainListener) {
            mMainListener.testModuleStarted(mModuleContext);
            List<String> resultNames = new ArrayList<String>();
            if (mEnableGranularResults) {
                for (int i = 0; i < mAttemptInProgress + 1; i++) {
                    List<TestRunResult> runResults = getTestRunForAttempts(i);
                    for (TestRunResult runResult : runResults) {
                        forwardRunResults(runResult, i);
                        resultNames.add(runResult.getName());
                    }
                }
            } else {
                for (TestRunResult runResult : getMergedTestRunResults()) {
                    // Forward the run level results
                    forwardRunResults(runResult, 0);
                    resultNames.add(runResult.getName());
                }
            }

            // Ensure we don't carry results from one module to another.
            for (String name : resultNames) {
                clearResultsForName(name);
            }
            mMainListener.testModuleEnded();
        }
        mModuleContext = null;
    }

    /** {@inheritDoc} */
    @Override
    public void invocationEnded(long elapsedTime) {
        super.invocationEnded(elapsedTime);
        synchronized (mMainListener) {
            logShardContent(getMergedTestRunResults());
            // Report all logs not associated with test runs
            forwardLogAssociation(getNonAssociatedLogFiles(), mMainListener);
            mMainListener.invocationEnded(elapsedTime);
        }
    }

    private void forwardRunResults(TestRunResult runResult, int attempt) {
        mMainListener.testRunStarted(
                runResult.getName(),
                runResult.getExpectedTestCount(),
                attempt,
                runResult.getStartTime());
        forwardTestResults(runResult.getTestResults());
        if (runResult.isRunFailure()) {
            mMainListener.testRunFailed(runResult.getRunFailureDescription());
        }

        // Provide a strong association of the run to its logs.
        forwardLogAssociation(runResult.getRunLoggedFiles(), mMainListener);

        mMainListener.testRunEnded(runResult.getElapsedTime(), runResult.getRunProtoMetrics());
    }

    private void forwardTestResults(Map<TestDescription, TestResult> testResults) {
        for (Map.Entry<TestDescription, TestResult> testEntry : testResults.entrySet()) {
            mMainListener.testStarted(testEntry.getKey(), testEntry.getValue().getStartTime());
            switch (testEntry.getValue().getStatus()) {
                case FAILURE:
                    mMainListener.testFailed(testEntry.getKey(), testEntry.getValue().getFailure());
                    break;
                case ASSUMPTION_FAILURE:
                    mMainListener.testAssumptionFailure(
                            testEntry.getKey(), testEntry.getValue().getStackTrace());
                    break;
                case IGNORED:
                    mMainListener.testIgnored(testEntry.getKey());
                    break;
                default:
                    break;
            }
            // Provide a strong association of the test to its logs.
            forwardLogAssociation(testEntry.getValue().getLoggedFiles(), mMainListener);

            if (!testEntry.getValue().getStatus().equals(TestStatus.INCOMPLETE)) {
                mMainListener.testEnded(
                        testEntry.getKey(),
                        testEntry.getValue().getEndTime(),
                        testEntry.getValue().getProtoMetrics());
            }
        }
    }

    /** Forward to the listener the logAssociated callback on the files. */
    private void forwardLogAssociation(
            MultiMap<String, LogFile> loggedFiles, ITestInvocationListener listener) {
        for (String key : loggedFiles.keySet()) {
            for (LogFile logFile : loggedFiles.get(key)) {
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener).logAssociation(key, logFile);
                }
            }
        }
    }

    /** Forward test cases logged files. */
    private void forwardLogAssociation(
            Map<String, LogFile> loggedFiles, ITestInvocationListener listener) {
        for (Entry<String, LogFile> logFile : loggedFiles.entrySet()) {
            if (listener instanceof ILogSaverListener) {
                ((ILogSaverListener) listener).logAssociation(logFile.getKey(), logFile.getValue());
            }
        }
    }

    /** Log the content of the shard for easier debugging. */
    private void logShardContent(Collection<TestRunResult> listResults) {
        StringBuilder sb = new StringBuilder();
        sb.append("=================================================\n");
        sb.append(
                String.format(
                        "========== Shard Primary Device %s ==========\n",
                        getInvocationContext().getDevices().get(0).getSerialNumber()));
        for (TestRunResult runRes : listResults) {
            sb.append(
                    String.format(
                            "\tRan '%s' in %s\n",
                            runRes.getName(), TimeUtil.formatElapsedTime(runRes.getElapsedTime())));
        }
        sb.append("=================================================\n");
        CLog.logAndDisplay(LogLevel.DEBUG, sb.toString());
    }
}
