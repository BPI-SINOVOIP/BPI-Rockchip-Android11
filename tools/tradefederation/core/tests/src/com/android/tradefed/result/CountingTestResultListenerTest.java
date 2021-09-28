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

import static com.google.common.truth.Truth.assertThat;

import com.android.ddmlib.testrunner.TestResult.TestStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link TestResultListener}. */
@RunWith(JUnit4.class)
public class CountingTestResultListenerTest {

    private CountingTestResultListener mResultListener;
    private TestDescription mTest;

    @Before
    public void setUp() {
        mResultListener = new CountingTestResultListener();
        mTest = new TestDescription("FooTest", "testFoo");
    }

    @Test
    public void testPassed() {
        mResultListener.testResult(mTest, createTestResult(TestStatus.PASSED));
        assertThat(mResultListener.getTotalTests()).isEqualTo(1);
        assertThat(mResultListener.getResultCounts()[TestStatus.PASSED.ordinal()]).isEqualTo(1);
        assertThat(mResultListener.getResultCounts()[TestStatus.FAILURE.ordinal()]).isEqualTo(0);
    }

    @Test
    public void testFailed() {
        mResultListener.testResult(mTest, createTestResult(TestStatus.FAILURE));
        assertThat(mResultListener.getTotalTests()).isEqualTo(1);
        assertThat(mResultListener.getResultCounts()[TestStatus.PASSED.ordinal()]).isEqualTo(0);
        assertThat(mResultListener.getResultCounts()[TestStatus.FAILURE.ordinal()]).isEqualTo(1);
    }

    @Test
    public void duplicateResults() {
        mResultListener.testResult(mTest, createTestResult(TestStatus.INCOMPLETE));
        assertThat(mResultListener.getResultCounts()[TestStatus.INCOMPLETE.ordinal()]).isEqualTo(1);
        mResultListener.testResult(mTest, createTestResult(TestStatus.PASSED));
        assertThat(mResultListener.getTotalTests()).isEqualTo(1);
        assertThat(mResultListener.getResultCounts()[TestStatus.PASSED.ordinal()]).isEqualTo(1);
        assertThat(mResultListener.getResultCounts()[TestStatus.INCOMPLETE.ordinal()]).isEqualTo(0);
    }

    @Test
    public void hasFailedTests() {
        assertThat(mResultListener.hasFailedTests()).isFalse();
        mResultListener.testResult(mTest, createTestResult(TestStatus.INCOMPLETE));
        assertThat(mResultListener.hasFailedTests()).isTrue();
        mResultListener.testResult(mTest, createTestResult(TestStatus.FAILURE));
        assertThat(mResultListener.hasFailedTests()).isTrue();
        mResultListener.testResult(mTest, createTestResult(TestStatus.ASSUMPTION_FAILURE));
        assertThat(mResultListener.hasFailedTests()).isTrue();
        mResultListener.testResult(mTest, createTestResult(TestStatus.PASSED));
        assertThat(mResultListener.hasFailedTests()).isFalse();
    }

    private TestResult createTestResult(TestStatus status) {
        TestResult result = new TestResult();
        result.setStatus(status);
        return result;
    }
}
