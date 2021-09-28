/*
 * Copyright (C) 2010 The Android Open Source Project
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
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.retry.MergeStrategy;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A {@link ITestInvocationListener} that will collect all test results.
 *
 * <p>Although the data structures used in this object are thread-safe, the {@link
 * ITestInvocationListener} callbacks must be called in the correct order.
 */
public class CollectingTestListener implements ITestInvocationListener, ILogSaverListener {

    @Option(
            name = "aggregate-metrics",
            description = "attempt to add test metrics values for test runs with the same name.")
    private boolean mIsAggregateMetrics = false;

    /** Toggle the 'aggregate metrics' option */
    protected void setIsAggregrateMetrics(boolean aggregate) {
        mIsAggregateMetrics = aggregate;
    }

    private IInvocationContext mContext;
    private IBuildInfo mBuildInfo;
    private Map<String, IInvocationContext> mModuleContextMap = new HashMap<>();
    // Use LinkedHashMap to provide consistent iterations over the keys.
    private Map<String, List<TestRunResult>> mTestRunResultMap =
            Collections.synchronizedMap(new LinkedHashMap<>());

    private IInvocationContext mCurrentModuleContext = null;
    private TestRunResult mCurrentTestRunResult = new TestRunResult();
    /** True if the default initialized mCurrentTestRunResult has its original value. */
    private boolean mDefaultRun = true;
    /** Track whether or not a test run is currently in progress */
    private boolean mRunInProgress = false;

    private Map<String, LogFile> mNonAssociatedLogFiles = new LinkedHashMap<>();

    // Tracks if mStatusCounts are accurate, or if they need to be recalculated
    private AtomicBoolean mIsCountDirty = new AtomicBoolean(true);
    // Tracks if the expected count is accurate, or if it needs to be recalculated.
    private AtomicBoolean mIsExpectedCountDirty = new AtomicBoolean(true);
    private int mExpectedCount = 0;

    // Represents the merged test results. This should not be accessed directly since it's only
    // calculated when needed.
    private final List<TestRunResult> mMergedTestRunResults = new ArrayList<>();
    // Represents the number of tests in each TestStatus state of the merged test results. Indexed
    // by TestStatus.ordinal()
    private int[] mStatusCounts = new int[TestStatus.values().length];

    private MergeStrategy mStrategy = MergeStrategy.ONE_TESTCASE_PASS_IS_PASS;

    /** Sets the {@link MergeStrategy} to use when merging results. */
    public void setMergeStrategy(MergeStrategy strategy) {
        mStrategy = strategy;
    }

    /**
     * Return the primary build info that was reported via {@link
     * #invocationStarted(IInvocationContext)}. Primary build is the build returned by the first
     * build provider of the running configuration. Returns null if there is no context (no build to
     * test case).
     */
    public IBuildInfo getPrimaryBuildInfo() {
        if (mContext == null) {
            return null;
        } else {
            return mContext.getBuildInfos().get(0);
        }
    }

    /**
     * Return the invocation context that was reported via {@link
     * #invocationStarted(IInvocationContext)}
     */
    public IInvocationContext getInvocationContext() {
        return mContext;
    }

    /**
     * Returns the build info.
     *
     * @deprecated rely on the {@link IBuildInfo} from {@link #getInvocationContext()}.
     */
    @Deprecated
    public IBuildInfo getBuildInfo() {
        return mBuildInfo;
    }

    /**
     * Set the build info. Should only be used for testing.
     *
     * @deprecated Not necessary for testing anymore.
     */
    @VisibleForTesting
    @Deprecated
    public void setBuildInfo(IBuildInfo buildInfo) {
        mBuildInfo = buildInfo;
    }

    /** {@inheritDoc} */
    @Override
    public void invocationStarted(IInvocationContext context) {
        mContext = context;
        mBuildInfo = getPrimaryBuildInfo();
    }

    /** {@inheritDoc} */
    @Override
    public void invocationEnded(long elapsedTime) {
        // ignore
    }

    /** {@inheritDoc} */
    @Override
    public void invocationFailed(Throwable cause) {
        // ignore
    }

    @Override
    public void testModuleStarted(IInvocationContext moduleContext) {
        mCurrentModuleContext = moduleContext;
    }

    @Override
    public void testModuleEnded() {
        mCurrentModuleContext = null;
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String name, int numTests) {
        testRunStarted(name, numTests, 0);
    }

    private TestRunResult getNewRunResult() {
        TestRunResult result = new TestRunResult();
        if (mDefaultRun) {
            result = mCurrentTestRunResult;
            mDefaultRun = false;
        }
        result.setAggregateMetrics(mIsAggregateMetrics);
        return result;
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String name, int numTests, int attemptNumber) {
        testRunStarted(name, numTests, attemptNumber, System.currentTimeMillis());
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String name, int numTests, int attemptNumber, long startTime) {
        setCountDirty();
        // Only testRunStarted can affect the expected count.
        mIsExpectedCountDirty.set(true);

        // Associate the run name with the current module context
        if (mCurrentModuleContext != null) {
            mModuleContextMap.put(name, mCurrentModuleContext);
        }

        // Add the list of maps if the run doesn't exist
        if (!mTestRunResultMap.containsKey(name)) {
            mTestRunResultMap.put(name, new LinkedList<>());
        }
        List<TestRunResult> results = mTestRunResultMap.get(name);

        // Set the current test run result based on the attempt
        if (attemptNumber < results.size()) {
            if (results.get(attemptNumber) == null) {
                throw new RuntimeException(
                        "Test run results should never be null in internal structure.");
            }
        } else if (attemptNumber == results.size()) {
            // new run
            TestRunResult result = getNewRunResult();
            results.add(result);
        } else {
            int size = results.size();
            for (int i = size; i < attemptNumber; i++) {
                TestRunResult result = getNewRunResult();
                result.testRunStarted(name, numTests, startTime);
                String errorMessage =
                        String.format(
                                "Run attempt %s of %s did not exists, but got attempt %s."
                                        + " This is a placeholder for the missing attempt.",
                                i, name, attemptNumber);
                result.testRunFailed(errorMessage);
                result.testRunEnded(0L, new HashMap<String, Metric>());
                results.add(result);
            }
            // New current run
            TestRunResult newResult = getNewRunResult();
            results.add(newResult);
        }
        mCurrentTestRunResult = results.get(attemptNumber);

        mCurrentTestRunResult.testRunStarted(name, numTests, startTime);
        mRunInProgress = true;
    }

    /** {@inheritDoc} */
    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        setCountDirty();
        mCurrentTestRunResult.testRunEnded(elapsedTime, runMetrics);
        mRunInProgress = false;
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(String errorMessage) {
        setCountDirty();
        mCurrentTestRunResult.testRunFailed(errorMessage);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(FailureDescription failure) {
        setCountDirty();
        mCurrentTestRunResult.testRunFailed(failure);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStopped(long elapsedTime) {
        setCountDirty();
        mCurrentTestRunResult.testRunStopped(elapsedTime);
    }

    /** {@inheritDoc} */
    @Override
    public void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    /** {@inheritDoc} */
    @Override
    public void testStarted(TestDescription test, long startTime) {
        setCountDirty();
        mCurrentTestRunResult.testStarted(test, startTime);
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        setCountDirty();
        mCurrentTestRunResult.testEnded(test, endTime, testMetrics);
    }

    /** {@inheritDoc} */
    @Override
    public void testFailed(TestDescription test, String trace) {
        setCountDirty();
        mCurrentTestRunResult.testFailed(test, trace);
    }

    @Override
    public void testFailed(TestDescription test, FailureDescription failure) {
        setCountDirty();
        mCurrentTestRunResult.testFailed(test, failure);
    }

    @Override
    public void testAssumptionFailure(TestDescription test, String trace) {
        setCountDirty();
        mCurrentTestRunResult.testAssumptionFailure(test, trace);
    }

    @Override
    public void testIgnored(TestDescription test) {
        setCountDirty();
        mCurrentTestRunResult.testIgnored(test);
    }

    /** {@inheritDoc} */
    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        if (mRunInProgress) {
            mCurrentTestRunResult.testLogSaved(dataName, logFile);
        } else {
            mNonAssociatedLogFiles.put(dataName, logFile);
        }
    }

    /**
     * Gets the results for the current test run.
     *
     * <p>Note the results may not be complete. It is recommended to test the value of {@link
     * TestRunResult#isRunComplete()} and/or (@link TestRunResult#isRunFailure()} as appropriate
     * before processing the results.
     *
     * @return the {@link TestRunResult} representing data collected during last test run
     */
    public TestRunResult getCurrentRunResults() {
        return mCurrentTestRunResult;
    }

    /** Returns the total number of complete tests for all runs. */
    public int getNumTotalTests() {
        computeMergedResults();
        int total = 0;
        for (TestStatus s : TestStatus.values()) {
            total += mStatusCounts[s.ordinal()];
        }
        return total;
    }

    /**
     * Returns the number of expected tests count. Could differ from {@link #getNumTotalTests()} if
     * some tests did not run.
     */
    public synchronized int getExpectedTests() {
        // If expected count is not dirty, no need to do anything
        if (!mIsExpectedCountDirty.compareAndSet(true, false)) {
            return mExpectedCount;
        }

        computeMergedResults();
        mExpectedCount = 0;
        for (TestRunResult result : getMergedTestRunResults()) {
            mExpectedCount += result.getExpectedTestCount();
        }
        return mExpectedCount;
    }

    /** Returns the number of tests in given state for this run. */
    public int getNumTestsInState(TestStatus status) {
        computeMergedResults();
        return mStatusCounts[status.ordinal()];
    }

    /** Returns if the invocation had any failed or assumption failed tests. */
    public boolean hasFailedTests() {
        return getNumAllFailedTests() > 0;
    }

    /** Returns the total number of test runs in a failure state */
    public int getNumAllFailedTestRuns() {
        int count = 0;
        for (TestRunResult result : getMergedTestRunResults()) {
            if (result.isRunFailure()) {
                count++;
            }
        }
        return count;
    }

    /**
     * Returns the total number of tests in a failure state (only failed, assumption failures do not
     * count toward it).
     */
    public int getNumAllFailedTests() {
        return getNumTestsInState(TestStatus.FAILURE);
    }

    /**
     * Return the merged collection of results for all runs across different attempts.
     *
     * <p>If there are multiple results, each test run is merged, with the latest test result
     * overwriting test results of previous runs. Test runs are ordered by attempt number.
     *
     * <p>Metrics for the same attempt will be merged based on the preference set by {@code
     * aggregate-metrics}. The final metrics will be the metrics of the last attempt.
     */
    public List<TestRunResult> getMergedTestRunResults() {
        computeMergedResults();
        return new ArrayList<>(mMergedTestRunResults);
    }

    /**
     * Returns the results for all test runs.
     *
     * @deprecated Use {@link #getMergedTestRunResults()}
     */
    @Deprecated
    public Collection<TestRunResult> getRunResults() {
        return getMergedTestRunResults();
    }

    /**
     * Computes and stores the merged results and the total status counts since both operations are
     * expensive.
     */
    private synchronized void computeMergedResults() {
        // If not dirty, nothing to be done
        if (!mIsCountDirty.compareAndSet(true, false)) {
            return;
        }

        mMergedTestRunResults.clear();
        // Merge results
        if (mTestRunResultMap.isEmpty() && mCurrentTestRunResult.isRunFailure()) {
            // In case of early failure that is a bit untracked, still add it to the list to
            // not loose it.
            CLog.e(
                    "Early failure resulting in no testRunStart. Results might be inconsistent:"
                            + "\n%s",
                    mCurrentTestRunResult.getRunFailureMessage());
            mMergedTestRunResults.add(mCurrentTestRunResult);
        } else {
            for (Entry<String, List<TestRunResult>> results : mTestRunResultMap.entrySet()) {
                TestRunResult res = TestRunResult.merge(results.getValue(), mStrategy);
                if (res == null) {
                    // Merge can return null in case of results being empty.
                    CLog.w("No results for %s", results.getKey());
                } else {
                    mMergedTestRunResults.add(res);
                }
            }
        }
        // Reset counts
        for (TestStatus s : TestStatus.values()) {
            mStatusCounts[s.ordinal()] = 0;
        }

        // Calculate results
        for (TestRunResult result : mMergedTestRunResults) {
            for (TestStatus s : TestStatus.values()) {
                mStatusCounts[s.ordinal()] += result.getNumTestsInState(s);
            }
        }
    }

    /**
     * Keep dirty count as AtomicBoolean to ensure when accessed from another thread the state is
     * consistent.
     */
    private void setCountDirty() {
        mIsCountDirty.set(true);
    }

    /**
     * Return all the names for all the test runs.
     *
     * <p>These test runs may have run multiple times with different attempts.
     */
    public Collection<String> getTestRunNames() {
        return new ArrayList<String>(mTestRunResultMap.keySet());
    }

    /**
     * Gets all the attempts for a {@link TestRunResult} of a given test run.
     *
     * @param testRunName The name given by {{@link #testRunStarted(String, int)}.
     * @return All {@link TestRunResult} for a given test run, ordered by attempts.
     */
    public List<TestRunResult> getTestRunAttempts(String testRunName) {
        return mTestRunResultMap.get(testRunName);
    }

    /**
     * Gets all the results for a given attempt.
     *
     * @param attempt The attempt we want results for.
     * @return All {@link TestRunResult} for a given attempt.
     */
    public List<TestRunResult> getTestRunForAttempts(int attempt) {
        List<TestRunResult> allResultForAttempts = new ArrayList<>();
        for (Entry<String, List<TestRunResult>> runInfo : mTestRunResultMap.entrySet()) {
            if (attempt < runInfo.getValue().size()) {
                TestRunResult attemptRes = runInfo.getValue().get(attempt);
                allResultForAttempts.add(attemptRes);
            }
        }
        return allResultForAttempts;
    }

    /**
     * Returns whether a given test run name has any results.
     *
     * @param testRunName The name given by {{@link #testRunStarted(String, int)}.
     */
    public boolean hasTestRunResultsForName(String testRunName) {
        return mTestRunResultMap.containsKey(testRunName);
    }

    /**
     * Returns the number of attempts for a given test run name.
     *
     * @param testRunName The name given by {{@link #testRunStarted(String, int)}.
     */
    public int getTestRunAttemptCount(String testRunName) {
        List<TestRunResult> results = mTestRunResultMap.get(testRunName);
        if (results == null) {
            return 0;
        }
        return results.size();
    }

    /**
     * Return the {@link TestRunResult} for a single attempt.
     *
     * @param testRunName The name given by {{@link #testRunStarted(String, int)}.
     * @param attempt The attempt id.
     * @return The {@link TestRunResult} for the given name and attempt id or {@code null} if it
     *     does not exist.
     */
    public TestRunResult getTestRunAtAttempt(String testRunName, int attempt) {
        List<TestRunResult> results = mTestRunResultMap.get(testRunName);
        if (results == null || attempt < 0 || attempt >= results.size()) {
            return null;
        }

        return results.get(attempt);
    }

    /**
     * Returns the {@link IInvocationContext} of the module associated with the results.
     *
     * @param testRunName The name given by {{@link #testRunStarted(String, int)}.
     * @return The {@link IInvocationContext} of the module for a given test run name {@code null}
     *     if there are no results for that name.
     */
    public IInvocationContext getModuleContextForRunResult(String testRunName) {
        return mModuleContextMap.get(testRunName);
    }

    /** Returns a copy of the map containing all the logged file not associated with a test run. */
    public Map<String, LogFile> getNonAssociatedLogFiles() {
        return new LinkedHashMap<>(mNonAssociatedLogFiles);
    }

    /**
     * Allows to clear the results for a given run name. Should only be used in some cases like the
     * aggregator of results.
     */
    protected final synchronized void clearResultsForName(String testRunName) {
        setCountDirty();
        mTestRunResultMap.remove(testRunName);
    }
}
