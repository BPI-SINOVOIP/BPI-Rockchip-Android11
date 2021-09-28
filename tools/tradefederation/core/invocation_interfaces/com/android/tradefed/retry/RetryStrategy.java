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

/** The Retry Strategy to be used when re-running some tests. */
public enum RetryStrategy {
    /** Do not attempt any retry */
    NO_RETRY,
    /** Rerun all the tests for the number of attempts specified. */
    ITERATIONS,
    /**
     * Rerun all the tests until the max count is reached or a failure occurs whichever come first.
     */
    RERUN_UNTIL_FAILURE,
    /**
     * Rerun all the test run and test cases failures until passed or the max number of attempts
     * specified. Test run failures are rerun in priority (a.k.a. if a run failure and a test case
     * failure occur, the run failure is rerun).
     */
    RETRY_ANY_FAILURE,
}
