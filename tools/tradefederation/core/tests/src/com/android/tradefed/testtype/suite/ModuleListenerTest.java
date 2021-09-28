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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestRunResult;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;
import java.util.List;

/** Unit tests for {@link ModuleListener} * */
@RunWith(JUnit4.class)
public class ModuleListenerTest {

    private ModuleListener mListener;
    private ITestInvocationListener mStubListener;

    @Before
    public void setUp() {
        mStubListener = new ITestInvocationListener() {};
        mListener = new ModuleListener(mStubListener);
    }

    /** Test that a regular execution yield the proper number of tests. */
    @Test
    public void testRegularExecution() {
        final int numTests = 5;
        mListener.testRunStarted("run1", numTests);
        for (int i = 0; i < numTests; i++) {
            TestDescription tid = new TestDescription("class", "test" + i);
            mListener.testStarted(tid);
            mListener.testEnded(tid, new HashMap<String, Metric>());
        }
        mListener.testRunEnded(0, new HashMap<String, Metric>());
        assertEquals(numTests, mListener.getNumTotalTests());
        assertEquals(numTests, mListener.getNumTestsInState(TestStatus.PASSED));
    }

    /** All the tests did not execute, so the amount of TestDescription seen is lower. */
    @Test
    public void testRun_missingTests() {
        final int numTests = 5;
        mListener.testRunStarted("run1", numTests);
        TestDescription tid = new TestDescription("class", "test" + numTests);
        // Only one test execute
        mListener.testStarted(tid);
        mListener.testEnded(tid, new HashMap<String, Metric>());
        mListener.testRunEnded(0, new HashMap<String, Metric>());

        // Expected and total tests differ because some tests did not execute
        assertEquals(numTests, mListener.getExpectedTests());
        assertEquals(1, mListener.getNumTotalTests());
        assertEquals(1, mListener.getNumTestsInState(TestStatus.PASSED));
    }

    /**
     * Some tests internally restart testRunStart to retry not_executed tests. We should not
     * aggregate the number of expected tests.
     */
    @Test
    public void testInternalRerun() {
        final int numTests = 5;
        mListener.testRunStarted("run1", numTests);
        TestDescription tid = new TestDescription("class", "test" + numTests);
        // Only one test execute the first time
        mListener.testStarted(tid);
        mListener.testEnded(tid, new HashMap<String, Metric>());
        mListener.testRunEnded(0, new HashMap<String, Metric>());

        // Runner restart to execute all the remaining
        mListener.testRunStarted("run1", numTests - 1);
        for (int i = 0; i < numTests - 1; i++) {
            TestDescription tid2 = new TestDescription("class", "test" + i);
            mListener.testStarted(tid2);
            mListener.testEnded(tid2, new HashMap<String, Metric>());
        }
        mListener.testRunEnded(0, new HashMap<String, Metric>());

        assertEquals(numTests, mListener.getNumTotalTests());
        assertEquals(numTests, mListener.getNumTestsInState(TestStatus.PASSED));
        // Expected count stays as 5
        assertEquals(numTests, mListener.getExpectedTests());
    }

    /** Some test runner calls testRunStart several times. We need to count all their tests. */
    @Test
    public void testMultiTestRun() {
        final int numTests = 5;
        mListener.testRunStarted("run1", numTests);
        for (int i = 0; i < numTests; i++) {
            TestDescription tid = new TestDescription("class", "test" + i);
            mListener.testStarted(tid);
            mListener.testEnded(tid, new HashMap<String, Metric>());
        }
        mListener.testRunEnded(0, new HashMap<String, Metric>());

        mListener.testRunStarted("run2", numTests);
        for (int i = 0; i < numTests; i++) {
            TestDescription tid = new TestDescription("class2", "test" + i);
            mListener.testStarted(tid);
            mListener.testEnded(tid, new HashMap<String, Metric>());
        }
        mListener.testRunEnded(0, new HashMap<String, Metric>());
        assertEquals(numTests * 2, mListener.getNumTotalTests());
        assertEquals(numTests * 2, mListener.getNumTestsInState(TestStatus.PASSED));
    }

    /**
     * Test that as long as a run crashed at attempt X, hasRunCrashedAtAttempt returns True at that
     * attempt.
     */
    @Test
    public void testhasRunCrashedAtAttempt() throws Exception {
        int maxRunLimit = 5;
        int clearRun1FailureAtAttempt = 3;
        int clearRun2FailureAtAttempt = 1;

        for (int attempt = 0; attempt < maxRunLimit; attempt++) {
            mListener.testRunStarted("run1", 0, attempt);
            if (attempt < clearRun1FailureAtAttempt) {
                mListener.testRunFailed("I failed!");
            }
            mListener.testRunEnded(0, new HashMap<String, Metric>());
            mListener.testRunStarted("run2", 0, attempt);
            if (attempt < clearRun2FailureAtAttempt) {
                mListener.testRunFailed("I failed!");
            }
            mListener.testRunEnded(0, new HashMap<String, Metric>());
        }
        int finalRunFailureAtAttempt =
                Math.max(clearRun1FailureAtAttempt, clearRun2FailureAtAttempt);
        for (int attempt = 0; attempt < finalRunFailureAtAttempt; attempt++) {
            assertTrue(hasRunCrashed(mListener.getTestRunForAttempts(attempt)));
        }
        for (int attempt = finalRunFailureAtAttempt; attempt < maxRunLimit; attempt++) {
            assertFalse(hasRunCrashed(mListener.getTestRunForAttempts(attempt)));
        }
    }

    /** Ensure that out of sequence attempts do not mess up reporting. */
    @Test
    public void testRetryInvalid() throws Exception {
        mListener.testRunStarted("apexservice_test", 1, 0);
        TestDescription test = new TestDescription("ApexServiceTest", "Activate");
        mListener.testStarted(test);
        mListener.testFailed(test, "failed");
        mListener.testEnded(test, new HashMap<String, Metric>());
        mListener.testRunEnded(500L, new HashMap<String, Metric>());
        mListener.testRunStarted("apex.test", 0, 1);
        mListener.testRunFailed("test.apex did not report any run.");

        mListener.testRunStarted("apexservice_test", 1, 1);
        mListener.testStarted(test);
        mListener.testEnded(test, new HashMap<String, Metric>());
        mListener.testRunEnded(500L, new HashMap<String, Metric>());

        List<TestRunResult> results = mListener.getMergedTestRunResults();
        assertEquals(2, results.size());
        assertEquals("apexservice_test", results.get(0).getName());
        assertFalse(results.get(0).isRunFailure());

        assertEquals("apex.test", results.get(1).getName());
        assertTrue(results.get(1).isRunFailure());
        assertEquals(
                "There were 2 failures:\n  Run attempt 0 of apex.test did not exists, but got "
                        + "attempt 1. This is a placeholder for the missing attempt.\n"
                        + "  test.apex did not report any run.",
                results.get(1).getRunFailureMessage());
    }

    private boolean hasRunCrashed(List<TestRunResult> results) {
        for (TestRunResult run : results) {
            if (run != null && run.isRunFailure()) {
                return true;
            }
        }
        return false;
    }
}
