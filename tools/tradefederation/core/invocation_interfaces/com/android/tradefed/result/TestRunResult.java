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

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.retry.MergeStrategy;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Holds results from a single test run.
 *
 * <p>Maintains an accurate count of tests, and tracks incomplete tests.
 *
 * <p>Not thread safe! The test* callbacks must be called in order
 */
public class TestRunResult {

    public static final String ERROR_DIVIDER = "\n====Next Error====\n";
    private String mTestRunName;
    // Uses a LinkedHashMap to have predictable iteration order
    private Map<TestDescription, TestResult> mTestResults =
            new LinkedHashMap<TestDescription, TestResult>();
    // Store the metrics for the run
    private Map<String, String> mRunMetrics = new HashMap<>();
    private HashMap<String, Metric> mRunProtoMetrics = new HashMap<>();
    // Log files associated with the test run itself (testRunStart / testRunEnd).
    private MultiMap<String, LogFile> mRunLoggedFiles;
    private boolean mIsRunComplete = false;
    private long mElapsedTime = 0L;
    private long mStartTime = 0L;

    private TestResult mCurrentTestResult;

    /** represents sums of tests in each TestStatus state. Indexed by TestStatus.ordinal() */
    private int[] mStatusCounts = new int[TestStatus.values().length];
    /** tracks if mStatusCounts is accurate, or if it needs to be recalculated */
    private boolean mIsCountDirty = true;

    private FailureDescription mRunFailureError = null;

    private boolean mAggregateMetrics = false;

    private int mExpectedTestCount = 0;

    /** Create an empty{@link TestRunResult}. */
    public TestRunResult() {
        mTestRunName = "not started";
        mRunLoggedFiles = new MultiMap<String, LogFile>();
    }

    public void setAggregateMetrics(boolean metricAggregation) {
        mAggregateMetrics = metricAggregation;
    }

    /** @return the test run name */
    public String getName() {
        return mTestRunName;
    }

    /** Returns a map of the test results. */
    public Map<TestDescription, TestResult> getTestResults() {
        return mTestResults;
    }

    /** @return a {@link Map} of the test run metrics. */
    public Map<String, String> getRunMetrics() {
        return mRunMetrics;
    }

    /** @return a {@link Map} of the test run metrics with the new proto format. */
    public HashMap<String, Metric> getRunProtoMetrics() {
        return mRunProtoMetrics;
    }

    /** Gets the set of completed tests. */
    public Set<TestDescription> getCompletedTests() {
        List<TestStatus> completedStatuses = new ArrayList<>();
        for (TestStatus s : TestStatus.values()) {
            if (!s.equals(TestStatus.INCOMPLETE)) {
                completedStatuses.add(s);
            }
        }
        return getTestsInState(completedStatuses);
    }

    /** Gets the set of failed tests. */
    public Set<TestDescription> getFailedTests() {
        return getTestsInState(Arrays.asList(TestStatus.FAILURE));
    }

    /** Gets the set of tests in given statuses. */
    private Set<TestDescription> getTestsInState(List<TestStatus> statuses) {
        Set<TestDescription> tests = new LinkedHashSet<>();
        for (Map.Entry<TestDescription, TestResult> testEntry : getTestResults().entrySet()) {
            TestStatus status = testEntry.getValue().getStatus();
            if (statuses.contains(status)) {
                tests.add(testEntry.getKey());
            }
        }
        return tests;
    }

    /** @return <code>true</code> if test run failed. */
    public boolean isRunFailure() {
        return mRunFailureError != null;
    }

    /** @return <code>true</code> if test run finished. */
    public boolean isRunComplete() {
        return mIsRunComplete;
    }

    public void setRunComplete(boolean runComplete) {
        mIsRunComplete = runComplete;
    }

    /**
     * Gets the number of test cases this TestRunResult expects to have. The actual number may be
     * less than the expected number due to test crashes. Normally, such a mismatch indicates a test
     * run failure.
     */
    public int getExpectedTestCount() {
        return mExpectedTestCount;
    }

    /** Gets the number of tests in given state for this run. */
    public int getNumTestsInState(TestStatus status) {
        if (mIsCountDirty) {
            // clear counts
            for (int i = 0; i < mStatusCounts.length; i++) {
                mStatusCounts[i] = 0;
            }
            // now recalculate
            for (TestResult r : mTestResults.values()) {
                mStatusCounts[r.getStatus().ordinal()]++;
            }
            mIsCountDirty = false;
        }
        return mStatusCounts[status.ordinal()];
    }

    /** Returns all the {@link TestResult} in a particular state. */
    public List<TestResult> getTestsResultsInState(TestStatus status) {
        List<TestResult> results = new ArrayList<>();
        for (TestResult r : mTestResults.values()) {
            if (r.getStatus().equals(status)) {
                results.add(r);
            }
        }
        return results;
    }

    /** Gets the number of tests in this run. */
    public int getNumTests() {
        return mTestResults.size();
    }

    /** Gets the number of complete tests in this run ie with status != incomplete. */
    public int getNumCompleteTests() {
        return getNumTests() - getNumTestsInState(TestStatus.INCOMPLETE);
    }

    /** @return <code>true</code> if test run had any failed or error tests. */
    public boolean hasFailedTests() {
        return getNumAllFailedTests() > 0;
    }

    /** Return total number of tests in a failure state (failed, assumption failure) */
    public int getNumAllFailedTests() {
        return getNumTestsInState(TestStatus.FAILURE);
    }

    /** Returns the current run elapsed time. */
    public long getElapsedTime() {
        return mElapsedTime;
    }

    /** Returns the start time of the first testRunStart call. */
    public long getStartTime() {
        return mStartTime;
    }

    /** Return the run failure error message, <code>null</code> if run did not fail. */
    public String getRunFailureMessage() {
        if (mRunFailureError == null) {
            return null;
        }
        return mRunFailureError.getErrorMessage();
    }

    /** Returns the run failure descriptor, <code>null</code> if run did not fail. */
    public FailureDescription getRunFailureDescription() {
        return mRunFailureError;
    }

    /**
     * Reset the run failure status.
     *
     * <p>Resetting the run failure status is sometimes required when retrying. This should be done
     * with care to avoid clearing a real failure.
     */
    public void resetRunFailure() {
        mRunFailureError = null;
    }

    /**
     * Notify that a test run started.
     *
     * @param runName the name associated to the test run for tracking purpose.
     * @param testCount the number of expected test cases associated with the test run.
     */
    public void testRunStarted(String runName, int testCount) {
        testRunStarted(runName, testCount, System.currentTimeMillis());
    }

    /**
     * Notify that a test run started.
     *
     * @param runName the name associated to the test run for tracking purpose.
     * @param testCount the number of expected test cases associated with the test run.
     */
    public void testRunStarted(String runName, int testCount, long startTime) {
        // A run may be started multiple times due to crashes or other reasons. Normally the first
        // run reflect the expected number of test "testCount". To avoid latter TestRunStarted
        // overrides the expected count, only the first testCount will be recorded.
        // mExpectedTestCount is initialized as 0.
        if (mExpectedTestCount == 0) {
            mExpectedTestCount = testCount;
        } else {
            CLog.w(
                    "%s calls testRunStarted more than once. Previous expected count: %s. "
                            + "New Expected count: %s",
                    runName, mExpectedTestCount, mExpectedTestCount + testCount);
            mExpectedTestCount += testCount;
        }
        mTestRunName = runName;
        mIsRunComplete = false;
        if (mStartTime == 0L) {
            mStartTime = startTime;
        }
        // Do not reset mRunFailureError since for re-run we want to preserve previous failures.
    }

    public void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    public void testStarted(TestDescription test, long startTime) {
        mCurrentTestResult = new TestResult();
        mCurrentTestResult.setStartTime(startTime);
        addTestResult(test, mCurrentTestResult);
    }

    private void addTestResult(TestDescription test, TestResult testResult) {
        mIsCountDirty = true;
        mTestResults.put(test, testResult);
    }

    private void updateTestResult(
            TestDescription test, TestStatus status, FailureDescription failure) {
        TestResult r = mTestResults.get(test);
        if (r == null) {
            CLog.d("received test event without test start for %s", test);
            r = new TestResult();
        }
        r.setStatus(status);
        if (failure != null) {
            r.setFailure(failure);
        }
        addTestResult(test, r);
    }

    public void testFailed(TestDescription test, String trace) {
        updateTestResult(test, TestStatus.FAILURE, FailureDescription.create(trace));
    }

    public void testFailed(TestDescription test, FailureDescription failure) {
        updateTestResult(test, TestStatus.FAILURE, failure);
    }

    public void testAssumptionFailure(TestDescription test, String trace) {
        updateTestResult(test, TestStatus.ASSUMPTION_FAILURE, FailureDescription.create(trace));
    }

    public void testIgnored(TestDescription test) {
        updateTestResult(test, TestStatus.IGNORED, null);
    }

    public void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    public void testEnded(TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        TestResult result = mTestResults.get(test);
        if (result == null) {
            result = new TestResult();
        }
        if (result.getStatus().equals(TestStatus.INCOMPLETE)) {
            result.setStatus(TestStatus.PASSED);
        }
        result.setEndTime(endTime);
        result.setMetrics(TfMetricProtoUtil.compatibleConvert(testMetrics));
        result.setProtoMetrics(testMetrics);
        addTestResult(test, result);
        mCurrentTestResult = null;
    }

    // TODO: Remove when done updating
    public void testRunFailed(String errorMessage) {
        if (errorMessage == null) {
            testRunFailed((FailureDescription) null);
        } else {
            testRunFailed(FailureDescription.create(errorMessage));
        }
    }

    public void testRunFailed(FailureDescription failureDescription) {
        if (failureDescription == null) {
            failureDescription = FailureDescription.create("testRunFailed(null) was called.");
        }

        if (mRunFailureError != null) {
            if (mRunFailureError instanceof MultiFailureDescription) {
                ((MultiFailureDescription) mRunFailureError).addFailure(failureDescription);
            } else {
                MultiFailureDescription aggregatedFailure =
                        new MultiFailureDescription(mRunFailureError, failureDescription);
                mRunFailureError = aggregatedFailure;
            }
        } else {
            mRunFailureError = failureDescription;
        }
    }

    public void testRunStopped(long elapsedTime) {
        mElapsedTime += elapsedTime;
        mIsRunComplete = true;
    }

    public void testRunEnded(long elapsedTime, Map<String, String> runMetrics) {
        if (mAggregateMetrics) {
            for (Map.Entry<String, String> entry : runMetrics.entrySet()) {
                String existingValue = mRunMetrics.get(entry.getKey());
                String combinedValue = combineValues(existingValue, entry.getValue());
                mRunMetrics.put(entry.getKey(), combinedValue);
            }
        } else {
            mRunMetrics.putAll(runMetrics);
        }
        // Also add to the new interface:
        mRunProtoMetrics.putAll(TfMetricProtoUtil.upgradeConvert(runMetrics));

        mElapsedTime += elapsedTime;
        mIsRunComplete = true;
    }

    /** New interface using the new proto metrics. */
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        // Internally store the information as backward compatible format
        testRunEnded(elapsedTime, TfMetricProtoUtil.compatibleConvert(runMetrics));
        // Store the new format directly too.
        // TODO: See if aggregation should/can be done with the new format.
        mRunProtoMetrics.putAll(runMetrics);

        // TODO: when old format is deprecated, do not forget to uncomment the next two lines
        // mElapsedTime += elapsedTime;
        // mIsRunComplete = true;
    }

    /**
     * Combine old and new metrics value
     *
     * @param existingValue
     * @param newValue
     * @return the combination of the two string as Long or Double value.
     */
    private String combineValues(String existingValue, String newValue) {
        if (existingValue != null) {
            try {
                Long existingLong = Long.parseLong(existingValue);
                Long newLong = Long.parseLong(newValue);
                return Long.toString(existingLong + newLong);
            } catch (NumberFormatException e) {
                // not a long, skip to next
            }
            try {
                Double existingDouble = Double.parseDouble(existingValue);
                Double newDouble = Double.parseDouble(newValue);
                return Double.toString(existingDouble + newDouble);
            } catch (NumberFormatException e) {
                // not a double either, fall through
            }
        }
        // default to overriding existingValue
        return newValue;
    }

    /** Returns a user friendly string describing results. */
    public String getTextSummary() {
        StringBuilder builder = new StringBuilder();
        builder.append(String.format("Total tests %d, ", getNumTests()));
        for (TestStatus status : TestStatus.values()) {
            int count = getNumTestsInState(status);
            // only add descriptive state for states that have non zero values, to avoid cluttering
            // the response
            if (count > 0) {
                builder.append(String.format("%s %d, ", status.toString().toLowerCase(), count));
            }
        }
        return builder.toString();
    }

    /**
     * Information about a file being logged are stored and associated to the test case or test run
     * in progress.
     *
     * @param dataName the name referencing the data.
     * @param logFile The {@link LogFile} object representing where the object was saved and and
     *     information about it.
     */
    public void testLogSaved(String dataName, LogFile logFile) {
        if (mCurrentTestResult != null) {
            // We have a test case in progress, we can associate the log to it.
            mCurrentTestResult.addLoggedFile(dataName, logFile);
        } else {
            mRunLoggedFiles.put(dataName, logFile);
        }
    }

    /** Returns a copy of the map containing all the logged file associated with that test case. */
    public MultiMap<String, LogFile> getRunLoggedFiles() {
        return new MultiMap<>(mRunLoggedFiles);
    }

    /** @see #merge(List, MergeStrategy) */
    public static TestRunResult merge(List<TestRunResult> testRunResults) {
        return merge(testRunResults, MergeStrategy.ONE_TESTCASE_PASS_IS_PASS);
    }

    /**
     * Merge multiple TestRunResults of the same testRunName. If a testcase shows up in multiple
     * TestRunResults but has different results (e.g. "boottest-device" runs three times with result
     * FAIL-FAIL-PASS), we concatenate all the stack traces from the FAILED runs and trust the final
     * run result for status, metrics, log files, start/end time.
     *
     * @param testRunResults A list of TestRunResult to merge.
     * @param strategy the merging strategy adopted for merging results.
     * @return the final TestRunResult containing the merged data from the testRunResults.
     */
    public static TestRunResult merge(List<TestRunResult> testRunResults, MergeStrategy strategy) {
        if (testRunResults.isEmpty()) {
            return null;
        }
        if (MergeStrategy.NO_MERGE.equals(strategy)) {
            throw new IllegalArgumentException(
                    "TestRunResult#merge cannot be called with NO_MERGE strategy.");
        }
        if (testRunResults.size() == 1) {
            // No merging is needed in case of a single test run result.
            return testRunResults.get(0);
        }
        TestRunResult finalRunResult = new TestRunResult();

        String testRunName = testRunResults.get(0).getName();
        Map<String, String> finalRunMetrics = new HashMap<>();
        HashMap<String, Metric> finalRunProtoMetrics = new HashMap<>();
        MultiMap<String, LogFile> finalRunLoggedFiles = new MultiMap<>();
        Map<TestDescription, List<TestResult>> testResultsAttempts = new LinkedHashMap<>();

        // Keep track of if one of the run is not complete
        boolean isAtLeastOneCompleted = false;
        boolean areAllCompleted = true;
        // Keep track of whether we have run failure or not
        List<FailureDescription> runErrors = new ArrayList<>();
        boolean atLeastOneFailure = false;
        boolean allFailure = true;
        // Keep track of elapsed time
        long elapsedTime = 0L;
        int maxExpectedTestCount = 0;

        for (TestRunResult eachRunResult : testRunResults) {
            // Check all mTestRunNames are the same.
            if (!testRunName.equals(eachRunResult.getName())) {
                throw new IllegalArgumentException(
                        String.format(
                                "Unabled to merge TestRunResults: The run results names are "
                                        + "different (%s, %s)",
                                testRunName, eachRunResult.getName()));
            }
            elapsedTime += eachRunResult.getElapsedTime();
            // Evaluate the run failures
            if (eachRunResult.isRunFailure()) {
                atLeastOneFailure = true;
                FailureDescription currentFailure = eachRunResult.getRunFailureDescription();
                if (currentFailure instanceof MultiFailureDescription) {
                    runErrors.addAll(((MultiFailureDescription) currentFailure).getFailures());
                } else {
                    runErrors.add(currentFailure);
                }
            } else {
                allFailure = false;
            }
            // Evaluate the run completion
            if (eachRunResult.isRunComplete()) {
                isAtLeastOneCompleted = true;
            } else {
                areAllCompleted = false;
            }

            // A run may start multiple times. Normally the first run shows the expected count
            // (max value).
            maxExpectedTestCount =
                    Math.max(maxExpectedTestCount, eachRunResult.getExpectedTestCount());

            // Keep the last TestRunResult's RunMetrics, ProtoMetrics
            finalRunMetrics.putAll(eachRunResult.getRunMetrics());
            finalRunProtoMetrics.putAll(eachRunResult.getRunProtoMetrics());
            finalRunLoggedFiles.putAll(eachRunResult.getRunLoggedFiles());
            // TODO: We are not handling the TestResult log files in the merging logic (different
            // from the TestRunResult log files). Need to improve in the future.
            for (Map.Entry<TestDescription, TestResult> testResultEntry :
                    eachRunResult.getTestResults().entrySet()) {
                if (!testResultsAttempts.containsKey(testResultEntry.getKey())) {
                    testResultsAttempts.put(testResultEntry.getKey(), new ArrayList<>());
                }
                List<TestResult> results = testResultsAttempts.get(testResultEntry.getKey());
                results.add(testResultEntry.getValue());
            }
        }

        // Evaluate test cases based on strategy
        finalRunResult.mTestResults = evaluateTestCases(testResultsAttempts, strategy);
        // Evaluate the run error status based on strategy
        boolean isRunFailure = isRunFailed(atLeastOneFailure, allFailure, strategy);
        if (isRunFailure) {
            if (runErrors.size() == 1) {
                finalRunResult.mRunFailureError = runErrors.get(0);
            } else {
                finalRunResult.mRunFailureError = new MultiFailureDescription(runErrors);
            }
        }
        // Evaluate run completion from all the attempts based on strategy
        finalRunResult.mIsRunComplete =
                isRunComplete(isAtLeastOneCompleted, areAllCompleted, strategy);

        finalRunResult.mTestRunName = testRunName;
        finalRunResult.mRunMetrics = finalRunMetrics;
        finalRunResult.mRunProtoMetrics = finalRunProtoMetrics;
        finalRunResult.mRunLoggedFiles = finalRunLoggedFiles;

        finalRunResult.mExpectedTestCount = maxExpectedTestCount;
        // Report total elapsed times
        finalRunResult.mElapsedTime = elapsedTime;
        return finalRunResult;
    }

    /** Merge the different test cases attempts based on the strategy. */
    private static Map<TestDescription, TestResult> evaluateTestCases(
            Map<TestDescription, List<TestResult>> results, MergeStrategy strategy) {
        Map<TestDescription, TestResult> finalTestResults = new LinkedHashMap<>();
        for (TestDescription description : results.keySet()) {
            List<TestResult> attemptRes = results.get(description);
            TestResult aggResult = TestResult.merge(attemptRes, strategy);
            finalTestResults.put(description, aggResult);
        }
        return finalTestResults;
    }

    /** Decides whether or not considering an aggregation of runs a pass or fail. */
    private static boolean isRunFailed(
            boolean atLeastOneFailure, boolean allFailures, MergeStrategy strategy) {
        switch (strategy) {
            case ANY_PASS_IS_PASS:
            case ONE_TESTRUN_PASS_IS_PASS:
                return allFailures;
            case ONE_TESTCASE_PASS_IS_PASS:
            case ANY_FAIL_IS_FAIL:
            default:
                return atLeastOneFailure;
        }
    }

    /** Decides whether or not considering an aggregation of runs completed or not. */
    private static boolean isRunComplete(
            boolean isAtLeastOneCompleted, boolean areAllCompleted, MergeStrategy strategy) {
        switch (strategy) {
            case ANY_PASS_IS_PASS:
            case ONE_TESTRUN_PASS_IS_PASS:
                return isAtLeastOneCompleted;
            case ONE_TESTCASE_PASS_IS_PASS:
            case ANY_FAIL_IS_FAIL:
            default:
                return areAllCompleted;
        }
    }
}
