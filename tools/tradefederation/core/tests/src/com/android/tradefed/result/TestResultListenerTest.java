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

import com.google.common.truth.Truth;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Collections;
import java.util.LinkedHashMap;

/** Unit tests for {@link TestResultListener}. */
@RunWith(JUnit4.class)
public class TestResultListenerTest {

    private LinkedHashMap<TestDescription, TestResult> mResults;
    private TestResultListener mResultListener;
    private TestDescription mTest;

    @Before
    public void setUp() {
        mResults = new LinkedHashMap<TestDescription, TestResult>();
        mResultListener =
                new TestResultListener() {
                    @Override
                    public void testResult(TestDescription test, TestResult result) {
                        Assert.assertFalse(mResults.containsKey(test));
                        mResults.put(test, result);
                    }
                };
        mTest = new TestDescription("FooTest", "testFoo");
    }

    @Test
    public void testPassed() {
        mResultListener.testStarted(mTest);
        mResultListener.testEnded(mTest, Collections.emptyMap());
        Truth.assertThat(mResults).containsEntry(mTest, createTestResult(TestStatus.PASSED));
        Truth.assertThat(mResults).hasSize(1);
    }

    @Test
    public void testFailed() {
        mResultListener.testStarted(mTest);
        mResultListener.testFailed(mTest, "foo");
        mResultListener.testEnded(mTest, Collections.emptyMap());
        TestResult result = createTestResult(TestStatus.FAILURE);
        result.setStackTrace("foo");
        Truth.assertThat(mResults).containsEntry(mTest, result);
        Truth.assertThat(mResults).hasSize(1);
    }

    @Test
    public void missingTestEnded() {
        TestDescription incompleteTest = new TestDescription("FooTest", "testFoo2");
        mResultListener.testStarted(incompleteTest);
        mResultListener.testStarted(mTest);
        mResultListener.testEnded(mTest, Collections.emptyMap());
        Truth.assertThat(mResults).containsEntry(mTest, createTestResult(TestStatus.PASSED));
        Truth.assertThat(mResults)
                .containsEntry(incompleteTest, createTestResult(TestStatus.INCOMPLETE));
        Truth.assertThat(mResults).hasSize(2);
    }

    @Test
    public void incompleteDueToCrash() {
        TestDescription incompleteTest = new TestDescription("FooTest", "testFoo2");
        mResultListener.testStarted(incompleteTest);
        mResultListener.testRunEnded(0, Collections.emptyMap());
        Truth.assertThat(mResults)
                .containsEntry(incompleteTest, createTestResult(TestStatus.INCOMPLETE));
        Truth.assertThat(mResults).hasSize(1);
    }

    private TestResult createTestResult(TestStatus status) {
        TestResult result = new TestResult();
        result.setStatus(status);
        return result;
    }
}
