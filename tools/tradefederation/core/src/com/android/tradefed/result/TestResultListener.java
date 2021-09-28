/*
 * Copyright (C) 2020 The Android Open Source Project
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
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;

import java.util.HashMap;
import java.util.Map;

/**
 * A simplification of ITestLifecycleListener for implementers that only care about individual test
 * results.
 *
 * <p>It filters the various lifecycle events down to a testResult method.
 *
 * <p>It is NOT thread safe - and in particular assumes that the ITestLifecycleListener events are
 * received in order.
 */
public abstract class TestResultListener implements ITestLifeCycleReceiver {

    private TestDescription mCurrentTest;
    private TestResult mCurrentResult;

    public abstract void testResult(TestDescription test, TestResult result);

    @Override
    public final void testStarted(TestDescription test, long startTime) {
        if (mCurrentTest != null) {
            // oh noes, previous test do not complete, forward an incomplete event
            reportTestFinish(null);
        }
        mCurrentTest = test;
        mCurrentResult = new TestResult();
        mCurrentResult.setStartTime(startTime);
    }

    @Override
    public final void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    @Override
    public final void testFailed(TestDescription test, String trace) {
        mCurrentResult.setStatus(TestStatus.FAILURE);
        mCurrentResult.setStackTrace(trace);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, String trace) {
        mCurrentResult.setStatus(TestStatus.ASSUMPTION_FAILURE);
        mCurrentResult.setStackTrace(trace);
    }

    @Override
    public final void testIgnored(TestDescription test) {
        mCurrentResult.setStatus(com.android.ddmlib.testrunner.TestResult.TestStatus.IGNORED);
    }

    @Override
    public final void testEnded(TestDescription test, Map<String, String> testMetrics) {
        mCurrentResult.setMetrics(testMetrics);
        reportTestFinish(test);
    }

    @Override
    public final void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        mCurrentResult.setProtoMetrics(testMetrics);
        reportTestFinish(test);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, Map<String, String> testMetrics) {
        mCurrentResult.setMetrics(testMetrics);
        reportTestFinish(test, endTime);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        mCurrentResult.setProtoMetrics(testMetrics);
        reportTestFinish(test, endTime);
    }

    @Override
    public void testRunEnded(long elapsedTimeMillis, HashMap<String, Metric> runMetrics) {
        if (mCurrentTest != null) {
            // last test did not finish! report incomplete
            mCurrentResult.setEndTime(System.currentTimeMillis());
            testResult(mCurrentTest, mCurrentResult);
            mCurrentTest = null;
            mCurrentResult = null;
        }
    }

    private void reportTestFinish(TestDescription test) {
        reportTestFinish(test, System.currentTimeMillis());
    }

    private void reportTestFinish(TestDescription test, long endTime) {
        if (mCurrentTest != null
                && mCurrentTest.equals(test)
                && mCurrentResult.getStatus() == TestStatus.INCOMPLETE) {
            mCurrentResult.setStatus(TestStatus.PASSED);
        }
        mCurrentResult.setEndTime(endTime);
        testResult(mCurrentTest, mCurrentResult);
        mCurrentTest = null;
        mCurrentResult = null;
    }
}
