/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.reporter;

import com.android.ddmlib.Log;
import com.android.ddmlib.Log.LogLevel;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.game.qualification.CertificationRequirements;
import com.android.game.qualification.metric.GameQualificationFpsCollector;
import com.android.game.qualification.metric.LoopSummary;
import com.android.game.qualification.metric.MetricSummary;
import com.android.tradefed.config.Option;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Result reporter for game core certification.
 */
public class GameQualificationResultReporter extends CollectingTestListener implements ILogSaverListener {
    private static final String TAG = GameQualificationResultReporter.class.getSimpleName();

    @Option(name = "suppress-passed-tests", description = "For functional tests, omit summary for "
            + "passing tests, only print failed and ignored ones")
    private boolean mSuppressPassedTest = false;

    private Map<TestDescription, MetricSummary> summaries = new ConcurrentHashMap<>();
    private Map<TestDescription, CertificationRequirements> mRequirements = new ConcurrentHashMap<>();
    private List<Throwable> invocationFailures = new ArrayList<>();
    private List<LogFile> mLogFiles = new ArrayList<>();
    private ILogSaver mLogSaver;
    private int mTotalAllocated = 0;

    public void putRequirements(TestDescription testId, CertificationRequirements requirements) {
        mRequirements.put(testId, requirements);
    }

    @Override
    public void invocationFailed(Throwable cause) {
        super.invocationFailed(cause);
        invocationFailures.add(cause);
    }
    /**
     * Collect metrics produces by
     * {@link GameQualificationFpsCollector}.
     */
    @Override
    public void testEnded(TestDescription testId, long elapsedTime, HashMap<String, Metric> metrics) {
        super.testEnded(testId, elapsedTime, metrics);
        if (!metrics.isEmpty()) {
                MetricSummary summary = MetricSummary.parseRunMetrics(getInvocationContext(), metrics);
                if (summary != null) {
                    summaries.put(testId, summary);
                } else if (metrics.containsKey("memory_allocated")) {
                    mTotalAllocated = (int) metrics.get("memory_allocated").getMeasurements().getSingleInt();
                }
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void testLogSaved(String dataName, LogDataType dataType, InputStreamSource dataStream,
            LogFile logFile) {
        super.testLogSaved(dataName, dataType, dataStream, logFile);
        mLogFiles.add(logFile);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setLogSaver(ILogSaver logSaver) {
        mLogSaver = logSaver;
    }

    @Override
    public void invocationEnded(long elapsedTime) {
        super.invocationEnded(elapsedTime);
        String summary = getInvocationSummary();

        Log.logAndDisplay(LogLevel.INFO, TAG, summary);
        try {
            mLogSaver.saveLogData(
                    "GameQualification-summary",
                    LogDataType.TEXT,
                    new ByteArrayInputStream(summary.getBytes()));
        } catch (IOException e) {
            Log.logAndDisplay(
                    LogLevel.ERROR, TAG, "Failed writing summary to file: " + e.getMessage());
        }
    }

    /**
     * Get the invocation summary as a string.
     */
    private String getInvocationSummary() {
        if (getMergedTestRunResults().isEmpty() && mLogFiles.isEmpty()) {
            return "No test results\n";
        }
        StringBuilder sb = new StringBuilder();

        // Print location of log files.
        if (!mLogFiles.isEmpty()) {
            sb.append("Log Files:\n");
            for (LogFile logFile : mLogFiles) {
                final String url = logFile.getUrl();
                sb.append(String.format("  %s\n", url != null ? url : logFile.getPath()));
            }
            sb.append("\n");
        }

        // Print invocation failures.
        if (!invocationFailures.isEmpty()) {
            sb.append("Unable to certify device.  Invocation failed:\n");
            for (Throwable t : invocationFailures) {
                sb.append("  ");
                sb.append(t.getMessage());
                sb.append("\n");
            }
            return sb.toString();
        }

        // Print out device test results.
        sb.append("Test Results:\n");
        for (TestRunResult testRunResult : getMergedTestRunResults()) {
            sb.append(getTestRunSummary(testRunResult));
        }

        // Print workload run metrics.
        for (Map.Entry<TestDescription, MetricSummary> entry : summaries.entrySet()) {
            sb.append(String.format("\n%s Metrics:\n%s\n", entry.getKey(), entry.getValue()));
        }

        // Print memory allocation metrics
        sb.append("Total Memory Allocated During Allocation Stress Test: ");
        sb.append(mTotalAllocated);
        sb.append("\n\n");

        // Determine certification level.
        sb.append("Certification:\n");

        boolean certified = true;

        sb.append("Functional tests [");
        sb.append(hasFailedTests() ? "FAILED" : "PASSED");
        sb.append("]\n");
        int testCount = 1;
        for (TestRunResult testRunResult : getMergedTestRunResults()) {
            for (Map.Entry<TestDescription, TestResult> entry
                    : testRunResult.getTestResults().entrySet()) {
                sb.append("\t[");
                sb.append(testCount);
                sb.append("/");
                sb.append(getNumTotalTests());
                sb.append("] ");
                sb.append(entry.getKey().toString());
                sb.append(": ");
                sb.append(entry.getValue().getStatus().name());
                sb.append("\n");
                testCount++;
            }
        }
        if (hasFailedTests()) {
            certified = false;
            sb.append("Certification failed because the following tests failed:\n");
            for (TestRunResult testRunResult : getMergedTestRunResults()) {
                for (TestDescription test : testRunResult.getFailedTests()) {
                    sb.append('\t');
                    sb.append(test.toString());

                    // Prints the first line of the stacktrace.
                    TestResult result = testRunResult.getTestResults().get(test);
                    String stacktrace = result.getStackTrace();
                    if (!stacktrace.isEmpty()) {
                        sb.append("\n\t\t");
                        sb.append(
                                stacktrace.contains("\n")
                                        ? stacktrace.split("\n", 2)[0]
                                        : stacktrace);
                    }
                    sb.append('\n');
                }
            }
            try {
                LogFile logFile = createFunctionalTestFailureReport();
                sb.append("Details can be found in ");
                sb.append(logFile.getPath());
                sb.append('\n');
            } catch (IOException e) {
                sb.append("Error generating test failures report: ");
                sb.append(e.getMessage());
            }
        }

        Report performanceReport = createPerformanceReport();
        sb.append("Performance tests [");
        sb.append(performanceReport.success ? "PASSED" : "FAILED");
        sb.append("]\n");
        sb.append(performanceReport.text);
        if (!performanceReport.success) {
            certified = false;
        }

        sb.append("\nGame Core Certification: ");
        sb.append(certified ? "PASSED" : "FAILED");
        return "Test results:\n" + sb.toString().trim() + "\n";
    }

    private LogFile createFunctionalTestFailureReport() throws IOException {
        StringBuilder sb = new StringBuilder();
        for (TestRunResult testRunResult : getMergedTestRunResults()) {
            for (TestDescription test : testRunResult.getFailedTests()) {
                sb.append("Test:\n");
                sb.append(test.toString());
                sb.append('\n');
                sb.append("Error:\n");
                TestResult result = testRunResult.getTestResults().get(test);
                sb.append(result.getStackTrace());
                sb.append("\n\n");
            }
        }
        return mLogSaver.saveLogData(
                "failure-report",
                LogDataType.TEXT,
                new ByteArrayInputStream(sb.toString().getBytes()));
    }

    /**
     * Get the test run summary as a string including run metrics.
     */
    private String getTestRunSummary(TestRunResult testRunResult) {
        StringBuilder sb = new StringBuilder();
        sb.append(String.format("%s:", testRunResult.getName()));
        if (testRunResult.getNumTests() > 0) {
            sb.append(String.format(" %d Test%s, %d Passed, %d Failed, %d Ignored",
                    testRunResult.getNumCompleteTests(),
                    testRunResult.getNumCompleteTests() == 1 ? "" : "s", // Pluralize Test
                    testRunResult.getNumTestsInState(TestStatus.PASSED),
                    testRunResult.getNumAllFailedTests(),
                    testRunResult.getNumTestsInState(TestStatus.IGNORED)));
        } else if (testRunResult.getRunMetrics().size() == 0) {
            sb.append(" No results");
        }
        sb.append("\n");
        Map<TestDescription, TestResult> testResults = testRunResult.getTestResults();
        for (Map.Entry<TestDescription, TestResult> entry : testResults.entrySet()) {
            if (mSuppressPassedTest && TestStatus.PASSED.equals(entry.getValue().getStatus())) {
                continue;
            }
            sb.append(getTestSummary(entry.getKey(), entry.getValue()));
        }
        sb.append("\n");
        return sb.toString();
    }

    private static class Report {
        boolean success;
        String text;

        public Report(boolean success, String text) {
            this.success = success;
            this.text = text;
        }
    }

    /**
     * Create a report on the performance metrics against certification requirements.
     *
     * Returns an empty String if all metrics passes certification.
     */
    private Report createPerformanceReport() {
        StringBuilder text = new StringBuilder();
        boolean success = true;
        for (Map.Entry<TestDescription, MetricSummary> entry : summaries.entrySet()) {
            TestDescription testId = entry.getKey();
            MetricSummary metrics =  entry.getValue();
            CertificationRequirements requirements = mRequirements.get(entry.getKey());
            if (requirements == null) {
                text.append("Warning: ");
                text.append(testId.getTestName());
                text.append(" was executed, but performance metrics was ignored because "
                        + "certification requirements was not found.\n");
            } else {
                List<LoopSummary> loopSummaries = metrics.getLoopSummaries();
                // Fail if any loop fails the jank rate test.  If there are 3 or more loops, ignore
                // the first and last loop.  Otherwise, check only the first loop.
                int begin = loopSummaries.size() > 2 ? 1 : 0;
                int end = loopSummaries.size() > 2 ? loopSummaries.size() - 1 : 1;
                for (int i = begin; i < end; i++) {
                    LoopSummary summary = loopSummaries.get(i);
                    if (summary.getJankRate() > requirements.getJankRate()) {
                        success = false;
                        text.append(
                                String.format(
                                        "Jank rate for %s in loop %d is too high, actual: "
                                                + "%f, target: %f\n",
                                        testId.getTestName(),
                                        i,
                                        summary.getJankRate(),
                                        requirements.getJankRate()));
                        break;
                    }
                }

                if (loopSummaries.size() == 2) {
                    text.append("Warning: runtime specification allowed for exactly 2 loops to be "
                            + "ran for ");
                    text.append(testId.getTestName());
                    text.append(".  Extending the runtime is recommended.");

                }

                if (requirements.getLoadTime() >= 0) {
                    if (metrics.getLoadTimeMs() == -1) {
                        success = false;
                        text.append("Unable to determine load time for ");
                        text.append(testId.getTestName());
                        text.append(".  Expected START_LOOP Intent was not received.\n");
                    } else if (metrics.getLoadTimeMs() > requirements.getLoadTime()) {
                        success = false;
                        text.append("Load time for ");
                        text.append(testId.getTestName());
                        text.append(" is too high, actual: ");
                        text.append(metrics.getLoadTimeMs());
                        text.append(" ms, target: ");
                        text.append(requirements.getLoadTime());
                        text.append(" ms\n");
                    }
                }
            }
        }
        return new Report(success, text.toString());
    }

    /**
     * Get the test summary as string including test metrics.
     */
    private String getTestSummary(TestDescription testId, TestResult testResult) {
        StringBuilder sb = new StringBuilder();
        sb.append(String.format("  %s: %s (%dms)\n", testId.toString(), testResult.getStatus(),
                testResult.getEndTime() - testResult.getStartTime()));
        String stack = testResult.getStackTrace();
        if (stack != null && !stack.isEmpty()) {
            sb.append("  stack=\n");
            String lines[] = stack.split("\\r?\\n");
            for (String line : lines) {
                sb.append(String.format("    %s\n", line));
            }
        }
        return sb.toString();
    }
}
