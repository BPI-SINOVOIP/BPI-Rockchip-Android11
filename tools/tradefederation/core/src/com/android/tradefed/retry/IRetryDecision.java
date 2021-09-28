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

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.IRemoteTest;

import java.util.List;

/**
 * Interface driving the retry decision and applying the filter on the class for more targeted
 * retry.
 */
public interface IRetryDecision {

    /** Whether or not to enable auto-retry. */
    public boolean isAutoRetryEnabled();

    /** The {@link com.android.tradefed.retry.RetryStrategy} used during auto-retry. */
    public RetryStrategy getRetryStrategy();

    /** Whether or not to reboot the device before the last attempt. */
    public boolean rebootAtLastAttempt();

    /** The maximum number of attempts during auto-retry. */
    public int getMaxRetryCount();

    /** Set the current invocation context. */
    public void setInvocationContext(IInvocationContext context);

    /**
     * Decide whether or not retry should be attempted. Also make any necessary changes to the
     * {@link IRemoteTest} to be retried (Applying filters, etc.).
     *
     * @param test The {@link IRemoteTest} that just ran.
     * @param attemptJustExecuted The number of the attempt that we just ran.
     * @param previousResults The list of {@link TestRunResult} of the test that just ran.
     * @return True if we should retry, False otherwise.
     * @throws DeviceNotAvailableException Can be thrown during device recovery
     */
    public boolean shouldRetry(
            IRemoteTest test, int attemptJustExecuted, List<TestRunResult> previousResults)
            throws DeviceNotAvailableException;

    /**
     * {@link #shouldRetry(IRemoteTest, int, List)} will most likely be called before the last retry
     * attempt, so we might be missing the very last attempt results for statistics purpose. This
     * method allows those results to be provided for proper statistics calculations.
     *
     * @param lastResults
     */
    public void addLastAttempt(List<TestRunResult> lastResults);

    /** Returns the {@link RetryStatistics} representing the retry. */
    public RetryStatistics getRetryStatistics();
}
