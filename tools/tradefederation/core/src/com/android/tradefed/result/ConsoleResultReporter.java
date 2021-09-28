/*
 * Copyright (C) 2015 The Android Open Source Project
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

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.invoker.IInvocationContext;

import java.io.PrintStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Result reporter to print the test results to the console.
 *
 * <p>Prints each test run, each test case, and test metrics, test logs, and test file locations.
 *
 * <p>
 */
@OptionClass(alias = "console-result-reporter")
public class ConsoleResultReporter extends TestResultListener
        implements ILogSaverListener, ITestInvocationListener {

    private static final SimpleDateFormat sTimeStampFormat = new SimpleDateFormat("HH:mm:ss");

    @Option(
            name = "suppress-passed-tests",
            description =
                    "For functional tests, ommit summary for "
                            + "passing tests, only print failed and ignored ones")
    private boolean mSuppressPassedTest = false;

    @Option(
            name = "display-failure-summary",
            description = "Display all the failures at the very end for easier visualization.")
    private boolean mDisplayFailureSummary = true;

    private final PrintStream mStream;
    private Set<LogFile> mLoggedFiles = new LinkedHashSet<>();
    private Map<TestDescription, TestResult> mFailures = new LinkedHashMap<>();
    private String mTestTag;
    private String mRunInProgress;
    private CountingTestResultListener mResultCountListener = new CountingTestResultListener();

    public ConsoleResultReporter() {
        this(System.out);
    }

    ConsoleResultReporter(PrintStream outputStream) {
        mStream = outputStream;
    }

    @Override
    public void invocationStarted(IInvocationContext context) {
        mTestTag = context.getTestTag();
    }

    @Override
    public void testResult(TestDescription test, TestResult result) {
        mResultCountListener.testResult(test, result);
        if (mSuppressPassedTest && TestStatus.PASSED.equals(result.getStatus())) {
            return;
        }
        if (mDisplayFailureSummary && TestStatus.FAILURE.equals(result.getStatus())) {
            mFailures.put(test, result);
        }
        print(getTestSummary(mTestTag, test, result));
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        super.testRunStarted(runName, testCount);
        mRunInProgress = runName;
    }

    @Override
    public void testRunFailed(String errorMessage) {
        print(String.format("%s: run failed: %s\n", mRunInProgress, errorMessage));
    }

    @Override
    public void testRunEnded(long elapsedTimeMillis, Map<String, String> metrics) {
        super.testRunEnded(elapsedTimeMillis, metrics);
        if (metrics != null && !metrics.isEmpty()) {
            String tag = mTestTag != null ? mTestTag : "unknown";
            String runName = mRunInProgress != null ? mRunInProgress : "unknown";
            StringBuilder sb = new StringBuilder(tag);
            sb.append(": ");
            sb.append(runName);
            sb.append(": ");
            List<String> metricKeys = new ArrayList<String>(metrics.keySet());
            Collections.sort(metricKeys);
            for (String metricKey : metricKeys) {
                sb.append(String.format("%s=%s\n", metricKey, metrics.get(metricKey)));
            }
            print(sb.toString());
        }
        mRunInProgress = null;
    }

    /** {@inheritDoc} */
    @Override
    public void invocationEnded(long elapsedTime) {
        int[] results = mResultCountListener.getResultCounts();
        StringBuilder sb = new StringBuilder();
        sb.append("========== Result Summary ==========");
        sb.append(String.format("\nResults summary for test-tag '%s': ", mTestTag));
        sb.append(mResultCountListener.getTotalTests());
        sb.append(" Tests [");
        sb.append(results[TestStatus.PASSED.ordinal()]);
        sb.append(" Passed");
        if (results[TestStatus.FAILURE.ordinal()] > 0) {
            sb.append(" ");
            sb.append(results[TestStatus.FAILURE.ordinal()]);
            sb.append(" Failed");
        }
        if (results[TestStatus.IGNORED.ordinal()] > 0) {
            sb.append(" ");
            sb.append(results[TestStatus.IGNORED.ordinal()]);
            sb.append(" Ignored");
        }
        if (results[TestStatus.ASSUMPTION_FAILURE.ordinal()] > 0) {
            sb.append(" ");
            sb.append(results[TestStatus.ASSUMPTION_FAILURE.ordinal()]);
            sb.append(" Assumption failures");
        }
        if (results[TestStatus.INCOMPLETE.ordinal()] > 0) {
            sb.append(" ");
            sb.append(results[TestStatus.INCOMPLETE.ordinal()]);
            sb.append(" Incomplete");
        }
        sb.append("] \r\n");
        print(sb.toString());
        if (mDisplayFailureSummary) {
            for (Entry<TestDescription, TestResult> entry : mFailures.entrySet()) {
                print(getTestSummary(mTestTag, entry.getKey(), entry.getValue()));
            }
        }
        // Print the logs
        for (LogFile logFile : mLoggedFiles) {
            printLog(logFile);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        mLoggedFiles.add(logFile);
    }

    /** {@inheritDoc} */
    @Override
    public void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        mLoggedFiles.add(logFile);
    }

    private void printLog(LogFile logFile) {
        if (mSuppressPassedTest && !mResultCountListener.hasFailedTests()) {
            // all tests passed, skip logging
            return;
        }
        String logDesc = logFile.getUrl() == null ? logFile.getPath() : logFile.getUrl();
        print("Log: " + logDesc + "\r\n");
    }

    /** Get the test summary as string including test metrics. */
    static String getTestSummary(String testTag, TestDescription testId, TestResult testResult) {
        StringBuilder sb = new StringBuilder();
        sb.append(
                String.format(
                        "%s: %s: %s (%dms)\n",
                        testTag,
                        testId.toString(),
                        testResult.getStatus(),
                        testResult.getEndTime() - testResult.getStartTime()));
        String stack = testResult.getStackTrace();
        if (stack != null && !stack.isEmpty()) {
            sb.append("  stack=\n");
            String lines[] = stack.split("\\r?\\n");
            for (String line : lines) {
                sb.append(String.format("    %s\n", line));
            }
        }
        Map<String, String> metrics = testResult.getMetrics();
        if (metrics != null && !metrics.isEmpty()) {
            List<String> metricKeys = new ArrayList<String>(metrics.keySet());
            Collections.sort(metricKeys);
            for (String metricKey : metricKeys) {
                sb.append(String.format("    %s: %s\n", metricKey, metrics.get(metricKey)));
            }
        }

        return sb.toString();
    }

    private void print(String msg) {
        mStream.print(sTimeStampFormat.format(new Date()) + " " + msg);
    }
}
