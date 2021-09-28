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

/** An Extension of {@link ResultForwarder} that always push to a given attempt number. */
public class RetryResultForwarder extends ResultForwarder {
    private int mAttemptNumber = 0;

    public RetryResultForwarder(int attemptNumber, ITestInvocationListener... listeners) {
        super(listeners);
        mAttemptNumber = attemptNumber;
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        super.testRunStarted(runName, testCount, mAttemptNumber);
    }

    @Override
    public void testRunStarted(String runName, int testCount, int attemptNumber) {
        // We enforce our attempt number
        super.testRunStarted(runName, testCount, mAttemptNumber);
    }
}
