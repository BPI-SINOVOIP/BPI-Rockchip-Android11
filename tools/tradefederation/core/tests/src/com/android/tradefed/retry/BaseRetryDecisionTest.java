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
package com.android.tradefed.retry;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link BaseRetryDecision}. */
@RunWith(JUnit4.class)
public class BaseRetryDecisionTest {

    private BaseRetryDecision mRetryDecision;
    private TestFilterableClass mTestClass;

    private class TestFilterableClass implements IRemoteTest, ITestFilterReceiver {

        private Set<String> mIncludeFilters = new HashSet<>();
        private Set<String> mExcludeFilters = new HashSet<>();

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceNotAvailableException {
            // Do nothing
        }

        @Override
        public void addIncludeFilter(String filter) {
            mIncludeFilters.add(filter);
        }

        @Override
        public void addAllIncludeFilters(Set<String> filters) {
            mIncludeFilters.addAll(filters);
        }

        @Override
        public void addExcludeFilter(String filter) {
            mExcludeFilters.add(filter);
        }

        @Override
        public void addAllExcludeFilters(Set<String> filters) {
            mExcludeFilters.addAll(filters);
        }

        @Override
        public Set<String> getIncludeFilters() {
            return mIncludeFilters;
        }

        @Override
        public Set<String> getExcludeFilters() {
            return mExcludeFilters;
        }

        @Override
        public void clearIncludeFilters() {
            mIncludeFilters.clear();
        }

        @Override
        public void clearExcludeFilters() {
            mExcludeFilters.clear();
        }
    }

    @Before
    public void setUp() throws Exception {
        mTestClass = new TestFilterableClass();
        mRetryDecision = new BaseRetryDecision();
        mRetryDecision.setInvocationContext(new InvocationContext());
        OptionSetter setter = new OptionSetter(mRetryDecision);
        setter.setOptionValue("max-testcase-run-count", "3");
        setter.setOptionValue("retry-strategy", "RETRY_ANY_FAILURE");
    }

    @Test
    public void testShouldRetry() throws Exception {
        TestRunResult result = createResult(null, null);
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertFalse(res);
    }

    @Test
    public void testShouldRetry_failure() throws Exception {
        TestRunResult result = createResult(null, FailureDescription.create("failure2"));
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method2"));
    }

    @Test
    public void testShouldRetry_failure_nonRetriable() throws Exception {
        TestRunResult result =
                createResult(
                        FailureDescription.create("failure"),
                        FailureDescription.create("failure2").setRetriable(false));
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method"));
        assertEquals(1, mTestClass.getExcludeFilters().size());
        assertTrue(mTestClass.getExcludeFilters().contains("class#method2"));
    }

    @Test
    public void testShouldRetry_success() throws Exception {
        TestRunResult result = createResult(null, FailureDescription.create("failure2"));
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method2"));
        // Following retry is successful
        TestRunResult result2 = createResult(null, null);
        boolean res2 = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result2));
        assertFalse(res2);
    }

    @Test
    public void testShouldRetry_morefailure() throws Exception {
        TestRunResult result = createResult(null, FailureDescription.create("failure2"));
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method2"));
        // Following retry clear the originally failing method, so we don't retry more
        TestRunResult result2 = createResult(FailureDescription.create("failure"), null);
        boolean res2 = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result2));
        assertFalse(res2);
    }

    @Test
    public void testShouldRetry_runFailure() throws Exception {
        FailureDescription failure = FailureDescription.create("run failure");
        TestRunResult result = createResult(null, null, failure);
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(0, mTestClass.getIncludeFilters().size());
        assertEquals(0, mTestClass.getExcludeFilters().size());
    }

    @Test
    public void testShouldRetry_runFailure_nonRetriable() throws Exception {
        FailureDescription failure = FailureDescription.create("run failure");
        failure.setRetriable(false);
        TestRunResult result = createResult(null, null, failure);

        FailureDescription failure2 = FailureDescription.create("run failure2");
        failure2.setRetriable(false);
        TestRunResult result2 = createResult(null, null, failure2);

        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result, result2));
        assertFalse(res);
        assertEquals(0, mTestClass.getIncludeFilters().size());
        assertEquals(0, mTestClass.getExcludeFilters().size());
    }

    @Test
    public void testShouldRetry_multi_runFailure_nonRetriable() throws Exception {
        FailureDescription failure = FailureDescription.create("run failure");
        failure.setRetriable(false);
        TestRunResult result = createResult(null, null, failure);

        FailureDescription failure2 = FailureDescription.create("run failure2");
        TestRunResult result2 = createResult(null, null, failure2);

        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result, result2));
        // Skip retry due to the non-retriable failure.
        assertFalse(res);
    }

    @Test
    public void testShouldRetry_multilayer_morefailure() throws Exception {
        TestRunResult result = createResult(null, FailureDescription.create("failure2"));
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertTrue(res);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method2"));

        TestRunResult result2 =
                createResult(
                        FailureDescription.create("failure"),
                        FailureDescription.create("failure2"));
        boolean res2 = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result2));
        assertTrue(res2);
        assertEquals(1, mTestClass.getIncludeFilters().size());
        assertTrue(mTestClass.getIncludeFilters().contains("class#method2"));

        TestRunResult result3 = createResult(FailureDescription.create("failure"), null);
        boolean res3 = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result3));
        assertFalse(res3);
    }

    /** Ensure we abort the retry if there are too many failed test cases. */
    @Test
    public void testAbortTooManyFailures() throws Exception {
        TestRunResult result = new TestRunResult();
        result.testRunStarted("TEST", 80);
        for (int i = 0; i < 80; i++) {
            TestDescription test = new TestDescription("class", "method" + i);
            result.testStarted(test);
            result.testFailed(test, "failure" + i);
            result.testEnded(test, new HashMap<String, Metric>());
        }
        result.testRunEnded(500, new HashMap<String, Metric>());
        boolean res = mRetryDecision.shouldRetry(mTestClass, 0, Arrays.asList(result));
        assertFalse(res);
    }

    private TestRunResult createResult(FailureDescription failure1, FailureDescription failure2) {
        return createResult(failure1, failure2, null);
    }

    private TestRunResult createResult(
            FailureDescription failure1,
            FailureDescription failure2,
            FailureDescription runFailure) {
        TestRunResult result = new TestRunResult();
        result.testRunStarted("TEST", 2);
        if (runFailure != null) {
            result.testRunFailed(runFailure);
        }
        TestDescription test1 = new TestDescription("class", "method");
        result.testStarted(test1);
        if (failure1 != null) {
            result.testFailed(test1, failure1);
        }
        result.testEnded(test1, new HashMap<String, Metric>());
        TestDescription test2 = new TestDescription("class", "method2");
        result.testStarted(test2);
        if (failure2 != null) {
            result.testFailed(test2, failure2);
        }
        result.testEnded(test2, new HashMap<String, Metric>());
        result.testRunEnded(500, new HashMap<String, Metric>());
        return result;
    }
}
