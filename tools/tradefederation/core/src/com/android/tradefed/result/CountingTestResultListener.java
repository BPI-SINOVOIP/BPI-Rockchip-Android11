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

import java.util.Collection;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/** A {@link TestResultListener} that tracks the total number of tests by {@link TestStatus} */
public class CountingTestResultListener extends TestResultListener {

    /** Keep a map of results, to accurately track final results after reruns etc */
    private Map<TestDescription, TestStatus> mResults = new ConcurrentHashMap<>();

    @Override
    public void testResult(TestDescription test, TestResult result) {
        mResults.put(test, result.getStatus());
    }

    /**
     * Return the number of PASSED, INCOMPLETE, IGNORED etc tests.
     *
     * @return an array, indexed by TestStatus.ordinal(), that stores the number of tests with each
     *     status
     */
    public int[] getResultCounts() {
        // calculate the counts as an array indexed by ordinal because its easier/faster
        int[] results = new int[TestStatus.values().length];
        Collection<TestStatus> data = mResults.values();
        for (TestStatus status : data) {
            results[status.ordinal()]++;
        }
        return results;
    }

    /** Return the total number of tests executed. */
    public int getTotalTests() {
        return mResults.size();
    }

    /**
     * Helper method to determine if there any failing (one of Incomplete, AssumptionFailure,
     * Failure) results.
     */
    public boolean hasFailedTests() {
        int[] results = getResultCounts();
        return results[TestStatus.INCOMPLETE.ordinal()] > 0
                || results[TestStatus.ASSUMPTION_FAILURE.ordinal()] > 0
                || results[TestStatus.FAILURE.ordinal()] > 0;
    }
}
