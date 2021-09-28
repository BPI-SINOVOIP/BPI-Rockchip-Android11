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
package com.android.tradefed.testtype;

import com.android.tradefed.result.ITestInvocationListener;

/**
 * In case of an incomplete execution, {@link IRemoteTest} that implements this interface may report
 * their non-executed tests for improved reporting.
 */
public interface IReportNotExecuted {

    public static final String NOT_EXECUTED_FAILURE = "Test did not run. This is a placeholder.";

    /**
     * Report the non-executed tests to the main listener provided. They should be reported as
     * failed with the {@link #NOT_EXECUTED_FAILURE} message.
     *
     * @param listener the main listener where to report the non-executed results.
     */
    public default void reportNotExecuted(ITestInvocationListener listener) {}

    /**
     * Report the non-executed tests to the main listener provided. They should be reported as
     * failed with the {@link #NOT_EXECUTED_FAILURE} message.
     *
     * @param listener the main listener where to report the non-executed results.
     * @param message the message to be associated with the non-executed failure.
     */
    public default void reportNotExecuted(ITestInvocationListener listener, String message) {
        reportNotExecuted(listener);
    }
}
