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

import static org.junit.Assert.*;

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.retry.MergeStrategy;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TestResult} */
@RunWith(JUnit4.class)
public class TestResultTest {

    @Test
    public void testMerge_singleRes() {
        List<TestResult> testResults = new ArrayList<>();
        testResults.add(createResult(TestStatus.PASSED, null, 2, 5));
        TestResult finalRes =
                TestResult.merge(testResults, MergeStrategy.ONE_TESTCASE_PASS_IS_PASS);
        assertEquals(TestStatus.PASSED, finalRes.getStatus());
        assertEquals(2, finalRes.getStartTime());
        assertEquals(5, finalRes.getEndTime());
        assertEquals(null, finalRes.getStackTrace());
    }

    @Test
    public void testMerge_multiRes_aggPass() {
        List<TestResult> testResults = new ArrayList<>();
        testResults.add(createResult(TestStatus.PASSED, null, 2, 5));
        testResults.add(createResult(TestStatus.FAILURE, "failed", 6, 7));
        TestResult finalRes =
                TestResult.merge(testResults, MergeStrategy.ONE_TESTCASE_PASS_IS_PASS);
        // Merge Strategy leave it a PASSED
        assertEquals(TestStatus.PASSED, finalRes.getStatus());
        assertTrue(finalRes.getProtoMetrics().containsKey(TestResult.IS_FLAKY));
        assertEquals(2, finalRes.getStartTime());
        assertEquals(7, finalRes.getEndTime());
        assertEquals("failed", finalRes.getStackTrace());
    }

    @Test
    public void testMerge_allFailed_aggPass() {
        List<TestResult> testResults = new ArrayList<>();
        testResults.add(createResult(TestStatus.FAILURE, "failed", 2, 5));
        testResults.add(createResult(TestStatus.FAILURE, "failed", 6, 7));
        TestResult finalRes =
                TestResult.merge(testResults, MergeStrategy.ONE_TESTCASE_PASS_IS_PASS);
        // Merge Strategy stays FAILURE since only failures
        assertEquals(TestStatus.FAILURE, finalRes.getStatus());
        assertEquals(2, finalRes.getStartTime());
        assertEquals(7, finalRes.getEndTime());
        assertEquals("There were 2 failures:\n  failed\n  failed", finalRes.getStackTrace());
    }

    @Test
    public void testMerge_multiRes() {
        List<TestResult> testResults = new ArrayList<>();
        testResults.add(createResult(TestStatus.PASSED, null, 2, 5));
        testResults.add(createResult(TestStatus.FAILURE, "failed", 6, 7));
        TestResult finalRes = TestResult.merge(testResults, MergeStrategy.ONE_TESTRUN_PASS_IS_PASS);
        // Merge Strategy leave it a FAILURE
        assertEquals(TestStatus.FAILURE, finalRes.getStatus());
        assertEquals(2, finalRes.getStartTime());
        assertEquals(7, finalRes.getEndTime());
        assertEquals("failed", finalRes.getStackTrace());
    }

    @Test
    public void testMerge_multiRes_allPass() {
        List<TestResult> testResults = new ArrayList<>();
        testResults.add(createResult(TestStatus.PASSED, null, 2, 5));
        testResults.add(createResult(TestStatus.PASSED, null, 6, 7));
        TestResult finalRes = TestResult.merge(testResults, MergeStrategy.ONE_TESTRUN_PASS_IS_PASS);
        // Merge Strategy stays a PASSED since only passing tests.
        assertEquals(TestStatus.PASSED, finalRes.getStatus());
        assertEquals(2, finalRes.getStartTime());
        assertEquals(7, finalRes.getEndTime());
        assertEquals(null, finalRes.getStackTrace());
    }

    private TestResult createResult(TestStatus status, String stack, long startTime, long endTime) {
        TestResult result = new TestResult();
        result.setStatus(status);
        result.setStackTrace(stack);
        result.setStartTime(startTime);
        result.setEndTime(endTime);
        return result;
    }
}
