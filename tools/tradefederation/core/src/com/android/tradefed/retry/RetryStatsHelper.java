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

import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestRunResult;

import com.google.common.collect.Sets;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Calculate the retry statistics and metrics based on attempts comparison. */
final class RetryStatsHelper {

    private List<List<TestRunResult>> mResults = new ArrayList<>();
    private RetryStatistics mStats = new RetryStatistics();

    /** Add the results from the latest run to be tracked for statistics purpose. */
    public void addResultsFromRun(List<TestRunResult> mLatestResults) {
        if (!mResults.isEmpty()) {
            updateSuccess(mResults.get(mResults.size() - 1), mLatestResults);
        }
        mResults.add(mLatestResults);
    }

    /**
     * Calculate the retry statistics based on currently known results and return the associated
     * {@link RetryStatistics} to represent the results.
     */
    public RetryStatistics calculateStatistics() {
        if (!mResults.isEmpty()) {
            List<TestRunResult> attemptResults = mResults.get(mResults.size() - 1);
            Set<TestDescription> attemptFailures =
                    BaseRetryDecision.getFailedTestCases(attemptResults).keySet();
            mStats.mRetryFailure = attemptFailures.size();
        }
        return mStats;
    }

    private void updateSuccess(
            List<TestRunResult> previousResults, List<TestRunResult> latestResults) {
        Set<TestDescription> diff =
                Sets.difference(
                        BaseRetryDecision.getFailedTestCases(previousResults).keySet(),
                        BaseRetryDecision.getFailedTestCases(latestResults).keySet());
        mStats.mRetrySuccess += diff.size();
    }
}
