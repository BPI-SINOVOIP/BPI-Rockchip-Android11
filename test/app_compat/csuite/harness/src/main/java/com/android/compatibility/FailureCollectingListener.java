/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.compatibility;

import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

public final class FailureCollectingListener implements ITestInvocationListener {
    private String mTestTrace = null;

    @Override
    public void testFailed(TestDescription test, String trace) {
        setStackTrace(trace != null ? trace : "unknown failure");
    }

    @Override
    public void testAssumptionFailure(TestDescription test, String trace) {
        setStackTrace(trace != null ? trace : "unknown assumption failure");
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(String errorMessage) {
        setStackTrace(errorMessage);
    }

    /**
     * Fetches the stack trace if any.
     *
     * @return the stack trace.
     */
    public String getStackTrace() {
        return mTestTrace;
    }

    /**
     * Sets the stack trace.
     *
     * @param stackTrace {@link String} stack trace to set.
     */
    public void setStackTrace(String stackTrace) {
        this.mTestTrace = stackTrace;
    }
}
