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

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link CollectingTestListener}. */
@RunWith(JUnit4.class)
public final class CollectingTestListenerTest {

    private static final String METRIC_VALUE = "value";
    private static final String TEST_KEY = "key";
    private static final String METRIC_VALUE2 = "value2";
    private static final String RUN_KEY = "key2";

    private CollectingTestListener mCollectingTestListener;

    @Before
    public void setUp() throws Exception {
        mCollectingTestListener = new CollectingTestListener();
        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("fakeDevice", new BuildInfo());
        mCollectingTestListener.invocationStarted(context);
    }

    /** Test the listener under a single normal test run. */
    @Test
    public void testSingleRun() {
        final TestDescription test = injectTestRun("run", "testFoo", METRIC_VALUE, 0);
        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();

        assertThat(runResult.isRunComplete()).isTrue();
        assertThat(runResult.isRunFailure()).isFalse();
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(1);
        assertThat(runResult.getTestResults().get(test).getStatus()).isEqualTo(TestStatus.PASSED);
        assertThat(runResult.getTestResults().get(test).getStartTime()).isGreaterThan(0L);
        assertThat(runResult.getTestResults().get(test).getEndTime())
                .isGreaterThan(runResult.getTestResults().get(test).getStartTime() - 1);
    }

    /** Test the listener where test run has failed. */
    @Test
    public void testRunFailed() {
        mCollectingTestListener.testRunStarted("foo", 1);
        mCollectingTestListener.testRunFailed("error");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());
        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();
        assertThat(runResult.isRunComplete()).isTrue();
        assertThat(runResult.isRunFailure()).isTrue();
        assertThat(runResult.getRunFailureMessage()).isEqualTo("error");
    }

    /** Test the listener where test run has failed. */
    @Test
    public void testRunFailed_counting() {
        mCollectingTestListener.testRunStarted("foo1", 1);
        mCollectingTestListener.testRunFailed("error1");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());
        mCollectingTestListener.testRunStarted("foo2", 1);
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());
        mCollectingTestListener.testRunStarted("foo3", 1);
        mCollectingTestListener.testRunFailed("error3");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());
        assertThat(mCollectingTestListener.getNumAllFailedTestRuns()).isEqualTo(2);
    }

    /** Test the listener when invocation is composed of two test runs. */
    @Test
    public void testTwoRuns() {
        final TestDescription test1 = injectTestRun("run1", "testFoo1", METRIC_VALUE, 0);
        final TestDescription test2 = injectTestRun("run2", "testFoo2", METRIC_VALUE2, 0);
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        assertThat(mCollectingTestListener.getNumTestsInState(TestStatus.PASSED)).isEqualTo(2);
        assertThat(mCollectingTestListener.getRunResults()).hasSize(2);
        assertThat(mCollectingTestListener.getMergedTestRunResults()).hasSize(2);
        Iterator<TestRunResult> runIter =
                mCollectingTestListener.getMergedTestRunResults().iterator();
        final TestRunResult runResult1 = runIter.next();
        final TestRunResult runResult2 = runIter.next();

        assertThat(runResult1.getName()).isEqualTo("run1");
        assertThat(runResult2.getName()).isEqualTo("run2");
        assertThat(runResult1.getTestResults().get(test1).getStatus()).isEqualTo(TestStatus.PASSED);
        assertThat(runResult2.getTestResults().get(test2).getStatus()).isEqualTo(TestStatus.PASSED);
        assertThat(runResult1.getRunMetrics().get(RUN_KEY)).isEqualTo(METRIC_VALUE);
        assertThat(runResult1.getTestResults().get(test1).getMetrics().get(TEST_KEY))
                .isEqualTo(METRIC_VALUE);
        assertThat(runResult2.getTestResults().get(test2).getMetrics().get(TEST_KEY))
                .isEqualTo(METRIC_VALUE2);
    }

    /** Test the listener when invocation is composed of a re-executed test run. */
    @Test
    public void testReRun() {
        final TestDescription test1 = injectTestRun("run", "testFoo1", METRIC_VALUE, 0);
        final TestDescription test2 = injectTestRun("run", "testFoo2", METRIC_VALUE2, 0);
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        assertThat(mCollectingTestListener.getNumTestsInState(TestStatus.PASSED)).isEqualTo(2);
        assertThat(mCollectingTestListener.getMergedTestRunResults()).hasSize(1);
        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();
        assertThat(runResult.getNumTestsInState(TestStatus.PASSED)).isEqualTo(2);
        assertThat(runResult.getCompletedTests()).contains(test1);
        assertThat(runResult.getCompletedTests()).contains(test2);
    }

    /**
     * Test the listener when invocation is composed of a re-executed test run, containing the same
     * tests
     */
    @Test
    public void testReRun_overlap() {
        injectTestRun("run", "testFoo1", METRIC_VALUE, 0);
        injectTestRun("run", "testFoo1", METRIC_VALUE2, 0, true);
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(1);
        assertThat(mCollectingTestListener.getNumTestsInState(TestStatus.PASSED)).isEqualTo(0);
        assertThat(mCollectingTestListener.getNumTestsInState(TestStatus.FAILURE)).isEqualTo(1);
        assertThat(mCollectingTestListener.getMergedTestRunResults()).hasSize(1);
        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();
        assertThat(runResult.getNumTestsInState(TestStatus.PASSED)).isEqualTo(0);
        assertThat(runResult.getNumTestsInState(TestStatus.FAILURE)).isEqualTo(1);
        assertThat(runResult.getNumTests()).isEqualTo(1);
    }

    @Test
    public void testRun_attempts() {
        injectTestRun("run", "testFoo1", METRIC_VALUE, 0);
        injectTestRun("run", "testFoo1", METRIC_VALUE, 1);
        injectTestRun("run", "testFoo2", METRIC_VALUE, 2);
        injectTestRun("run", "testFoo2", METRIC_VALUE, 0);
        injectTestRun("run", "testFoo2", METRIC_VALUE, 1);
        assertThat(mCollectingTestListener.getTestRunNames()).containsExactly("run");
        assertThat(mCollectingTestListener.getTestRunAttemptCount("run")).isEqualTo(3);
        final TestRunResult run1 = mCollectingTestListener.getTestRunAtAttempt("run", 0);
        final TestRunResult run2 = mCollectingTestListener.getTestRunAtAttempt("run", 1);
        final TestRunResult run3 = mCollectingTestListener.getTestRunAtAttempt("run", 2);
        assertThat(mCollectingTestListener.getTestRunAttempts("run"))
                .containsExactly(run1, run2, run3);
        assertThat(run1.getNumTests()).isEqualTo(2);
        assertThat(run2.getNumTests()).isEqualTo(2);
        assertThat(run3.getNumTests()).isEqualTo(1);
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        // Check results which do not exist
        assertThat(mCollectingTestListener.getTestRunAttemptCount("notrun")).isEqualTo(0);
        assertThat(mCollectingTestListener.getTestRunAttempts("notrun")).isNull();
        assertThat(mCollectingTestListener.getTestRunAtAttempt("notrun", 0)).isNull();
        assertThat(mCollectingTestListener.getTestRunAtAttempt("run", 3)).isNull();
    }

    @Test
    public void testRun_invalidAttempts() {
        injectTestRun("run", "testFoo1", METRIC_VALUE, 0);
        injectTestRun("run", "testFoo1", METRIC_VALUE, 2);
        List<TestRunResult> results = mCollectingTestListener.getTestRunAttempts("run");
        assertEquals(3, results.size());
        assertFalse(results.get(0).isRunFailure());
        assertTrue(results.get(1).isRunFailure()); // Missing run is failed
        assertFalse(results.get(2).isRunFailure());
    }

    /** Test run with incomplete tests */
    @Test
    @SuppressWarnings("unchecked")
    public void testSingleRun_incomplete() {
        mCollectingTestListener.testRunStarted("run", 1);
        mCollectingTestListener.testStarted(new TestDescription("FooTest", "incomplete"));
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());
        assertThat(mCollectingTestListener.getNumTestsInState(TestStatus.INCOMPLETE)).isEqualTo(1);
    }

    /** Test aggregating of metrics with long values */
    @Test
    public void testRunEnded_aggregateLongMetrics() {
        mCollectingTestListener.setIsAggregrateMetrics(true);
        injectTestRun("run", "testFoo1", "1", 0);
        injectTestRun("run", "testFoo1", "1", 0);
        assertThat(mCollectingTestListener.getCurrentRunResults().getRunMetrics().get(RUN_KEY))
                .isEqualTo("2");
    }

    /** Test aggregating of metrics with double values */
    @Test
    public void testRunEnded_aggregateDoubleMetrics() {
        mCollectingTestListener.setIsAggregrateMetrics(true);
        injectTestRun("run", "testFoo1", "1.1", 0);
        injectTestRun("run", "testFoo1", "1.1", 0);
        assertThat(mCollectingTestListener.getCurrentRunResults().getRunMetrics().get(RUN_KEY))
                .isEqualTo("2.2");
    }

    /** Test aggregating of metrics with different data types */
    @Test
    public void testRunEnded_aggregateMixedMetrics() {
        mCollectingTestListener.setIsAggregrateMetrics(true);
        injectTestRun("run", "testFoo1", "1", 0);
        injectTestRun("run", "testFoo1", "1.1", 0);
        mCollectingTestListener.invocationEnded(0);
        assertThat(mCollectingTestListener.getCurrentRunResults().getRunMetrics().get(RUN_KEY))
                .isEqualTo("2.1");
    }

    /** Test aggregating of metrics when new metric isn't a number */
    @Test
    public void testRunEnded_aggregateNewStringMetrics() {
        mCollectingTestListener.setIsAggregrateMetrics(true);
        injectTestRun("run", "testFoo1", "1", 0);
        injectTestRun("run", "testFoo1", "bar", 0);
        mCollectingTestListener.invocationEnded(0);
        assertThat(mCollectingTestListener.getCurrentRunResults().getRunMetrics().get(RUN_KEY))
                .isEqualTo("bar");
    }

    /** Test aggregating of metrics when existing metric isn't a number */
    @Test
    public void testRunEnded_aggregateExistingStringMetrics() {
        mCollectingTestListener.setIsAggregrateMetrics(true);
        injectTestRun("run", "testFoo1", "bar", 0);
        injectTestRun("run", "testFoo1", "1", 0);
        mCollectingTestListener.invocationEnded(0);
        assertThat(mCollectingTestListener.getCurrentRunResults().getRunMetrics().get(RUN_KEY))
                .isEqualTo("1");
    }

    @Test
    public void testGetNumTestsInState() {
        injectTestRun("run", "testFoo1", "1", 0);
        injectTestRun("run", "testFoo2", "1", 0);
        int testsPassed = mCollectingTestListener.getNumTestsInState(TestStatus.PASSED);
        assertThat(testsPassed).isEqualTo(2);
        injectTestRun("run", "testFoo3", "1", 0);
        testsPassed = mCollectingTestListener.getNumTestsInState(TestStatus.PASSED);
        assertThat(testsPassed).isEqualTo(3);
    }

    @Test
    public void testGetNumTotalTests() {
        injectTestRun("run", "testFoo1", "1", 0);
        injectTestRun("run", "testFoo2", "1", 0);
        int total = mCollectingTestListener.getNumTotalTests();
        assertThat(total).isEqualTo(2);
        injectTestRun("run", "testFoo3", "1", 0, true);
        total = mCollectingTestListener.getNumTotalTests();
        assertThat(total).isEqualTo(3);
    }

    @Test
    public void testEarlyFailure() {
        CollectingTestListener listener = new CollectingTestListener();
        listener.testRunFailed("early failure!");
        List<TestRunResult> results = listener.getMergedTestRunResults();
        assertThat(results.size()).isEqualTo(1);
        TestRunResult res = results.get(0);
        // We are in an odd state, but we should not loose the failure.
        assertThat(res.isRunFailure()).isTrue();
        assertThat(res.getName()).isEqualTo("not started");
    }

    /** Test the listener under a single normal test run that gets sharded */
    @Test
    public void testSingleRun_multi() {
        mCollectingTestListener.testRunStarted("run1", 1);
        final TestDescription test = new TestDescription("FooTest", "testName1");
        mCollectingTestListener.testStarted(test);
        mCollectingTestListener.testEnded(test, new HashMap<String, Metric>());
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        mCollectingTestListener.testRunStarted("run1", 3);
        final TestDescription test2 = new TestDescription("FooTest", "testName2");
        mCollectingTestListener.testStarted(test2);
        mCollectingTestListener.testEnded(test2, new HashMap<String, Metric>());
        mCollectingTestListener.testRunFailed("missing tests");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();

        assertThat(runResult.isRunComplete()).isTrue();
        assertThat(runResult.isRunFailure()).isTrue();
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        assertThat(mCollectingTestListener.getExpectedTests()).isEqualTo(4);
    }

    /** Test the listener under a single normal test run that gets sharded */
    @Test
    public void testSingleRun_multi_failureRunFirst() {
        mCollectingTestListener.testRunStarted("run1", 3);
        final TestDescription test2 = new TestDescription("FooTest", "testName2");
        mCollectingTestListener.testStarted(test2);
        mCollectingTestListener.testEnded(test2, new HashMap<String, Metric>());
        mCollectingTestListener.testRunFailed("missing tests");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        mCollectingTestListener.testRunStarted("run1", 1);
        final TestDescription test = new TestDescription("FooTest", "testName1");
        mCollectingTestListener.testStarted(test);
        mCollectingTestListener.testEnded(test, new HashMap<String, Metric>());
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        TestRunResult runResult = mCollectingTestListener.getCurrentRunResults();

        assertThat(runResult.isRunComplete()).isTrue();
        assertThat(runResult.isRunFailure()).isTrue();
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        assertThat(mCollectingTestListener.getExpectedTests()).isEqualTo(4);
    }

    @Test
    public void testSingleRun_multi_failureRunFirst_attempts() {
        mCollectingTestListener.testRunStarted("run1", 3);
        final TestDescription test2 = new TestDescription("FooTest", "testName2");
        mCollectingTestListener.testStarted(test2);
        mCollectingTestListener.testEnded(test2, new HashMap<String, Metric>());
        mCollectingTestListener.testRunFailed("missing tests");
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        mCollectingTestListener.testRunStarted("run1", 1, 1);
        final TestDescription test = new TestDescription("FooTest", "testName1");
        mCollectingTestListener.testStarted(test);
        mCollectingTestListener.testEnded(test, new HashMap<String, Metric>());
        mCollectingTestListener.testRunEnded(0, new HashMap<String, Metric>());

        TestRunResult runResult = mCollectingTestListener.getMergedTestRunResults().get(0);

        assertThat(runResult.isRunComplete()).isTrue();
        assertThat(runResult.isRunFailure()).isTrue();
        assertThat(mCollectingTestListener.getNumTotalTests()).isEqualTo(2);
        // Accross attempt we do not increment the total expected test
        assertThat(mCollectingTestListener.getExpectedTests()).isEqualTo(3);
    }

    /**
     * Injects a single test run with 1 passed test into the {@link CollectingTestListener} under
     * test
     *
     * @return the {@link TestDescription} of added test
     */
    private TestDescription injectTestRun(
            String runName, String testName, String metricValue, int attempt) {
        return injectTestRun(runName, testName, metricValue, attempt, false);
    }

    /**
     * Injects a single test run with 1 test into the {@link CollectingTestListener} under test.
     *
     * @return the {@link TestDescription} of added test
     */
    private TestDescription injectTestRun(
            String runName, String testName, String metricValue, int attempt, boolean failtest) {
        Map<String, String> runMetrics = new HashMap<String, String>(1);
        runMetrics.put(RUN_KEY, metricValue);
        Map<String, String> testMetrics = new HashMap<String, String>(1);
        testMetrics.put(TEST_KEY, metricValue);

        mCollectingTestListener.testRunStarted(runName, 1, attempt);
        final TestDescription test = new TestDescription("FooTest", testName);
        mCollectingTestListener.testStarted(test);
        if (failtest) {
            mCollectingTestListener.testFailed(test, "trace");
        }
        mCollectingTestListener.testEnded(test, TfMetricProtoUtil.upgradeConvert(testMetrics));
        mCollectingTestListener.testRunEnded(0, TfMetricProtoUtil.upgradeConvert(runMetrics));
        return test;
    }
}
