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
package com.android.tradefed.testtype.suite.retry;

import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.suite.retry.RetryRescheduler.RetryType;

import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Helper class to determine which module or test should run or not. */
public final class RetryResultHelper {

    /** Returns whether or not a module should be re-run. */
    public static boolean shouldRunModule(TestRunResult moduleResults, List<RetryType> types) {
        if (moduleResults.isRunFailure() && types.contains(RetryType.NOT_EXECUTED)) {
            // module has not_executed tests that should be re-run
            return true;
        }
        // If module has some states that needs to be retried.
        for (TestStatus status : getStatusesToRun(types)) {
            if (moduleResults.getNumTestsInState(status) > 0) {
                return true;
            }
        }
        return false;
    }

    /** Returns whether or not a test case should be run or not. */
    public static boolean shouldRunTest(TestResult result, List<RetryType> types) {
        if (getStatusesToRun(types).contains(result.getStatus())) {
            return true;
        }
        return false;
    }

    private static Set<TestStatus> getStatusesToRun(List<RetryType> types) {
        Set<TestStatus> statusesToRun = new LinkedHashSet<>();
        if (types.contains(RetryType.FAILED)) {
            statusesToRun.add(TestStatus.FAILURE);
            statusesToRun.add(TestStatus.INCOMPLETE);
        }
        if (types.contains(RetryType.NOT_EXECUTED)) {
            statusesToRun.add(TestStatus.INCOMPLETE);
        }
        return statusesToRun;
    }
}
