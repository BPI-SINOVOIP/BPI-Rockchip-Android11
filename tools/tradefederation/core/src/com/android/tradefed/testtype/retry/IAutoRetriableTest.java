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
package com.android.tradefed.testtype.retry;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;

import java.util.List;

/**
 * Interface for an {@link IRemoteTest} that doesn't implement {@link ITestFilterReceiver} but still
 * wishes to support auto-retry.
 *
 * <p>The recommendation for most runners is to implement {@link ITestFilterReceiver} and give
 * granular control over what tests are running for the harness to handle. But in some situation, it
 * might not be possible and some delegated form of retry is necessary.
 */
public interface IAutoRetriableTest extends IRemoteTest {

    /**
     * Delegated from {@link IRetryDecision#shouldRetry(IRemoteTest, int, List)}. Decide whether or
     * not retry should be attempted. Also make any necessary changes to the {@link IRemoteTest} to
     * be retried (Applying filters, preparing next run, etc.).
     *
     * @param attemptJustExecuted The number of the attempt that we just ran.
     * @param previousResults The list of {@link TestRunResult} of the test that just ran.
     * @return True if we should retry, False otherwise.
     * @throws DeviceNotAvailableException Can be thrown during device recovery
     */
    public default boolean shouldRetry(int attemptJustExecuted, List<TestRunResult> previousResults)
            throws DeviceNotAvailableException {
        // No retry by default
        return false;
    }

    /**
     * Recovery attempt on the device to get it a better state before next retry. Will only be
     * triggered if {@link #shouldRetry(int, List)} returns true.
     *
     * @param devices The list of {@link ITestDevice} to apply recovery on.
     * @param attemptJustExecuted The number of the attempt that we just ran.
     */
    public default void recoverStateOfDevices(List<ITestDevice> devices, int attemptJustExecuted) {
        // Do nothing by default
    }
}
