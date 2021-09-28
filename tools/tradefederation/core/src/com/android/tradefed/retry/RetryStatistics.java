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

import com.android.tradefed.testtype.IRemoteTest;

import java.util.List;

/**
 * Structure holding the statistics for a retry session of one {@link IRemoteTest}. Not all fields
 * might be populated depending of the {@link com.android.tradefed.retry.RetryStrategy}.
 */
public class RetryStatistics {
    // The time spent in retry. Always populated if retries or iterations occurred
    public long mRetryTime = 0L;

    // Success and failure counts. Populated for RETRY_ANY_FAILURE.
    public long mRetrySuccess = 0L;
    public long mRetryFailure = 0L;

    /** Helper method to aggregate the statistics of several retries. */
    public static final RetryStatistics aggregateStatistics(List<RetryStatistics> stats) {
        RetryStatistics aggregatedStats = new RetryStatistics();
        for (RetryStatistics s : stats) {
            aggregatedStats.mRetryTime += s.mRetryTime;
            aggregatedStats.mRetrySuccess += s.mRetrySuccess;
            aggregatedStats.mRetryFailure += s.mRetryFailure;
        }
        return aggregatedStats;
    }
}
