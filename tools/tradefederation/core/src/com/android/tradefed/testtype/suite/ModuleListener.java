/*
 * Copyright (C) 2017 The Android Open Source Project
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
import com.android.tradefed.result.LogSaverResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IRemoteTest;

import java.util.HashMap;

/**
 * Listener attached to each {@link IRemoteTest} of each module in order to collect the list of
 * results.
 */
public class ModuleListener extends CollectingTestListener {

    private boolean mSkip = false;
    private TestStatus mTestStatus;
    private String mTrace;
    private int mTestsRan = 1;
    private ITestInvocationListener mMainListener;

    private boolean mCollectTestsOnly = false;
    /** Track runs in progress for logging purpose */
    private boolean mRunInProgress = false;

    /** Constructor. */
    public ModuleListener(ITestInvocationListener listener) {
        mMainListener = listener;
        mRunInProgress = false;
        setIsAggregrateMetrics(true);
    }

    /** Sets whether or not we are only collecting the tests. */
    public void setCollectTestsOnly(boolean collectTestsOnly) {
        mCollectTestsOnly = collectTestsOnly;
    }

    @Override
    public void testRunStarted(String name, int numTests, int attemptNumber) {
        testRunStarted(name, numTests, attemptNumber, System.currentTimeMillis());
    }

    @Override
    public void testRunStarted(String name, int numTests, int attemptNumber, long startTime) {
        mRunInProgress = true;
        // In case of retry of the same run, do not add the expected count again. This allows
        // situation where test runner has a built-in retry (like InstrumentationTest) and calls
        // testRunStart several times to be counted properly.
        if (getTestRunAtAttempt(name, attemptNumber) != null) {
            numTests = 0;
        }
        super.testRunStarted(name, numTests, attemptNumber, startTime);
        if (attemptNumber != 0) {
            mTestsRan = 1;
        }
        CLog.d("ModuleListener.testRunStarted(%s, %s, %s)", name, numTests, attemptNumber);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(String errorMessage) {
        CLog.d("ModuleListener.testRunFailed(%s)", errorMessage);
        super.testRunFailed(errorMessage);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(FailureDescription failure) {
        CLog.d("ModuleListener.testRunFailed(%s)", failure.toString());
        super.testRunFailed(failure);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        super.testRunEnded(elapsedTime, runMetrics);
        mRunInProgress = false;
    }

    /** Returns whether or not the listener last retry session has failed. */
    public boolean hasLastAttemptFailed() {
        return getCurrentRunResults().isRunFailure();
    }

    /** {@inheritDoc} */
    @Override
    public void testStarted(TestDescription test, long startTime) {
        if (!mCollectTestsOnly) {
            CLog.d("ModuleListener.testStarted(%s)", test.toString());
        }
        mTestStatus = TestStatus.PASSED;
        mTrace = null;
        super.testStarted(test, startTime);
        if (mSkip) {
            super.testIgnored(test);
            mTestStatus = TestStatus.IGNORED;
        }
    }

    /** Helper to log the test passed if it didn't fail. */
    private void logTestStatus(TestDescription testName, TestStatus status) {
        if (!mCollectTestsOnly) {
            String runName = "";
            // Only print the run name in addition to test case fully qualified if different.
            if (!testName.getClassName().startsWith(getCurrentRunResults().getName())) {
                runName = getCurrentRunResults().getName() + " ";
            }
            String runAndTestCase = String.format("%s%s", runName, testName.toString());
            String message =
                    String.format(
                            "[%d/%d] %s %s", mTestsRan, getExpectedTests(), runAndTestCase, status);
            if (mTrace != null) {
                message += ": " + mTrace;
            }
            CLog.logAndDisplay(LogLevel.INFO, message);
        }
        mTestsRan++;
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        logTestStatus(test, mTestStatus);
        super.testEnded(test, endTime, testMetrics);
    }

    @Override
    public void testIgnored(TestDescription test) {
        super.testIgnored(test);
        mTestStatus = TestStatus.IGNORED;
    }

    @Override
    public void testAssumptionFailure(TestDescription test, String trace) {
        super.testAssumptionFailure(test, trace);
        mTestStatus = TestStatus.ASSUMPTION_FAILURE;
        mTrace = trace;
    }

    @Override
    public void testAssumptionFailure(TestDescription test, FailureDescription failure) {
        super.testAssumptionFailure(test, failure);
        mTestStatus = TestStatus.ASSUMPTION_FAILURE;
        mTrace = failure.toString();
    }

    /** {@inheritDoc} */
    @Override
    public void testFailed(TestDescription test, String trace) {
        if (mSkip) {
            return;
        }
        mTestStatus = TestStatus.FAILURE;
        mTrace = trace;
        super.testFailed(test, trace);
    }

    /** {@inheritDoc} */
    @Override
    public void testFailed(TestDescription test, FailureDescription failure) {
        if (mSkip) {
            return;
        }
        mTestStatus = TestStatus.FAILURE;
        mTrace = failure.toString();
        super.testFailed(test, failure);
    }

    /** Whether or not to mark all the test cases skipped. */
    public void setMarkTestsSkipped(boolean skip) {
        mSkip = skip;
    }

    /** {@inheritDoc} */
    @Override
    public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        if (mMainListener instanceof LogSaverResultForwarder) {
            // If the listener is a log saver, we should simply forward the testLog not save again.
            ((LogSaverResultForwarder) mMainListener)
                    .testLogForward(dataName, dataType, dataStream);
        } else {
            super.testLog(dataName, dataType, dataStream);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        // Forward to CollectingTestListener to store the logs
        super.testLogSaved(dataName, dataType, dataStream, logFile);
        // Forward to the main listener so logs are properly reported to the end result_reporters.
        if (mMainListener instanceof ILogSaverListener) {
            ((ILogSaverListener) mMainListener)
                    .testLogSaved(dataName, dataType, dataStream, logFile);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        if (mRunInProgress) {
            super.logAssociation(dataName, logFile);
        } else {
            // If no runs are in progress, any logs is reported at the module level.
            if (mMainListener instanceof ILogSaverListener) {
                ((ILogSaverListener) mMainListener).logAssociation(dataName, logFile);
            }
        }
    }
}
