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
package com.android.tradefed.retry;

/** Describes how the results should be aggregated when multiple attempts are present. */
public enum MergeStrategy {
    /** Merging should not be applied and will throw an exception. */
    NO_MERGE,
    /** If a single test case pass then we will consider the merged result passed. */
    ONE_TESTCASE_PASS_IS_PASS,
    /** If a single test run pass then we will consider the merged run result passed. */
    ONE_TESTRUN_PASS_IS_PASS,
    /** If a single run or test cases is a pass we will consider the merged results passed. */
    ANY_PASS_IS_PASS,
    /** If a single run or test cases is failed, status will be failed no matter what. */
    ANY_FAIL_IS_FAIL;

    /** Create a merge strategy based on the retry strategy. */
    public static MergeStrategy getMergeStrategy(RetryStrategy retryStrategy) {
        // TODO: Expand to take into account more context: postsubmit vs. presubmit
        MergeStrategy strategy = MergeStrategy.ONE_TESTCASE_PASS_IS_PASS;
        switch (retryStrategy) {
            case ITERATIONS:
                strategy = MergeStrategy.ANY_FAIL_IS_FAIL;
                break;
            case RERUN_UNTIL_FAILURE:
                strategy = MergeStrategy.ANY_FAIL_IS_FAIL;
                break;
            case RETRY_ANY_FAILURE:
                strategy = MergeStrategy.ANY_PASS_IS_PASS;
                break;
            case NO_RETRY:
                strategy = MergeStrategy.ANY_FAIL_IS_FAIL;
                break;
        }
        return strategy;
    }
}
